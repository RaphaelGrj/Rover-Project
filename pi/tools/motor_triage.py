"""One command that answers the only question that matters right now:
is the motor fault electrical, or is it in the firmware?

Why this exists
---------------
The answer has been available all along, but only to someone who runs
five SYSTEM commands in the right order and reads the telemetry
correctly. Seven bring-up sessions went by without that sequence being
run cleanly end to end -- and the one number everyone did look at
(`left_pwm=255`) turned out to prove nothing at all, because the PID
saturates precisely BECAUSE nothing moves.

So this runs the sequence itself and prints a verdict:

    SYSTEM action=resume          -- and report a refusal instead of ignoring it
    SYSTEM action=reset_ticks
    SYSTEM action=motor_raw       -- fixed duty, NO PID, NO encoder feedback
    SYSTEM action=raw_ticks

`motor_raw` is what makes this decisive: it writes the duty straight to
the H-bridge, bypassing the PID, the feed-forward, the speed ceiling and
the obstacle reflex. If the wheels do not turn under it, nothing
upstream of the H-bridge can be blamed and the fault is electrical.

Output is in French, unaccented: the verdict is what the operator acts
on, it is cross-read against PROGRESS.md which is French, and dropping
the accents keeps it readable on a Windows console. Code and comments
stay English per CLAUDE.md.

Examples (run from pi/):
  python -m tools.motor_triage --port /dev/ttyUSB0
  python -m tools.motor_triage --port socket://rover.lan:3333 --duration 5
  python -m tools.motor_triage --port COM10 --yes        # no confirmation prompt

WARNING: this commands FULL DUTY to both motors. Prop the robot up first.
"""
from __future__ import annotations

import argparse
import re
import sys
import threading
import time
from pathlib import Path

# Importable however it is launched -- `python -m tools.motor_triage` from
# pi/, or by path from anywhere. (The older move_diagnostic.py hardcoded
# one developer's Windows path here, which stopped working the day the Pi
# became the machine running it.)
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from rover_esp32.link import RoverLink  # noqa: E402

# Matches rover_core.core -- comfortably under the firmware's 500ms
# timeout. MOVE and motor_raw are both gated on ACTIVE, and ACTIVE is
# only held while heartbeats keep arriving.
HEARTBEAT_PERIOD_S = 0.15

# An edge count climbing this fast while the tick count stays flat means
# the input is oscillating, not the wheel. Mirrors
# Encoder::MAX_PLAUSIBLE_EDGES_PER_S (esp32/lib/motors/Encoder.h).
IMPLAUSIBLE_EDGES_PER_S = 20000

_FIRMWARE_HEADER = (
    Path(__file__).resolve().parent.parent.parent / "esp32" / "include" / "motion_config.h"
)


def _read_firmware_constant(name: str) -> float | None:
    """Reads a `constexpr float NAME = value;` out of motion_config.h.

    Duplicating those numbers here would be its own small disaster: this
    entire session started with a speed constant that was wrong by 10x
    and nobody noticing. Reading the real one means this tool cannot
    disagree with the firmware about what it just measured. Returns None
    if anything at all is unexpected -- the verdict does not depend on
    it, only the extra speed figure does.
    """
    try:
        source = _FIRMWARE_HEADER.read_text(encoding="utf-8")
    except OSError:
        return None
    match = re.search(rf"^constexpr float {name}\s*=\s*([0-9.eE+-]+)f?;", source, re.M)
    if match is None:
        return None
    try:
        return float(match.group(1))
    except ValueError:
        return None


class Telemetry:
    """Accumulates what the robot says, on the link's reader thread."""

    def __init__(self) -> None:
        self._lock = threading.Lock()
        # STATE fields merge (the firmware sends them as several
        # independent lines), EVENT/ERROR accumulate as occurrences.
        self.state: dict[str, str] = {}
        self.errors: list[dict[str, str]] = []
        self.events: list[dict[str, str]] = []

    def on_frame(self, frame_type: str, fields: dict[str, str]) -> None:
        with self._lock:
            if frame_type == "STATE":
                self.state.update(fields)
            elif frame_type == "ERROR":
                self.errors.append(dict(fields))
            elif frame_type == "EVENT":
                self.events.append(dict(fields))

    def snapshot(self) -> dict[str, str]:
        with self._lock:
            return dict(self.state)

    def error_codes(self) -> list[str]:
        with self._lock:
            return [e.get("code", "") for e in self.errors]

    def wait_for_fields(self, keys: tuple[str, ...], timeout: float) -> dict[str, str] | None:
        """Waits until every key has been seen at least once."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            snapshot = self.snapshot()
            if all(key in snapshot for key in keys):
                return snapshot
            time.sleep(0.05)
        return None

    def forget(self, *keys: str) -> None:
        """Drops fields so the next wait_for_fields sees the NEW reply
        rather than the one still sitting there from a previous step."""
        with self._lock:
            for key in keys:
                self.state.pop(key, None)


def _heartbeat_loop(link: RoverLink, stop: threading.Event) -> None:
    while not stop.is_set():
        link.send("HEARTBEAT")
        stop.wait(HEARTBEAT_PERIOD_S)


def _verdict(title: str, body: str) -> None:
    line = "=" * 72
    print(f"\n{line}\n  {title}\n{line}")
    for paragraph in body.strip().split("\n"):
        print(f"  {paragraph}")
    print(line)


def _run(args: argparse.Namespace) -> int:
    telemetry = Telemetry()
    link = RoverLink(args.port, secret=args.secret)
    link.on_frame = telemetry.on_frame
    link.start()

    stop_heartbeat = threading.Event()
    heartbeat = threading.Thread(
        target=_heartbeat_loop, args=(link, stop_heartbeat), daemon=True
    )
    heartbeat.start()
    try:
        return _triage(link, telemetry, args)
    finally:
        stop_heartbeat.set()
        heartbeat.join(timeout=1.0)
        link.stop()


def _triage(link: RoverLink, telemetry: Telemetry, args: argparse.Namespace) -> int:
    print(f"[1/5] Connexion a {args.port} ...")
    deadline = time.monotonic() + args.connect_timeout
    while time.monotonic() < deadline and not link.is_connected:
        time.sleep(0.05)
    if not link.is_connected:
        _verdict(
            "LIEN INJOIGNABLE",
            "Aucune connexion au robot.\n"
            "  - port ou URL incorrect ?\n"
            "  - ESP32 hors tension, ou cable USB defectueux (deja vu le 2026-09-01) ?\n"
            "Rien n'a ete teste. Le triage ne dit rien sur les moteurs.",
        )
        return 2

    # The firmware publishes state/estop/max_speed on change plus once at
    # boot, so a link that has just come up may not have seen it yet --
    # diag asks for it outright rather than waiting on a transition that
    # may never come.
    link.send("SYSTEM", {"action": "diag"})
    snapshot = telemetry.wait_for_fields(("state",), timeout=args.reply_timeout)
    if snapshot is None:
        _verdict(
            "PAS DE REPONSE A SYSTEM action=diag",
            "Le lien est ouvert mais le robot n'a pas repondu son etat (de la telemetrie\n"
            "peut tout de meme circuler : ce sont deux choses differentes).\n"
            "Causes usuelles : firmware trop ancien pour ce depot, boucle de reset, ou\n"
            "pair different de celui attendu au bout du socket.\n"
            "Rien n'a ete teste sur les moteurs.",
        )
        return 2

    print(f"      etat={snapshot.get('state')} estop={snapshot.get('estop', '?')}")

    print("[2/5] Armement (SYSTEM action=resume) ...")
    telemetry.forget("state")
    link.send("SYSTEM", {"action": "resume"})
    time.sleep(0.4)
    link.send("SYSTEM", {"action": "diag"})
    snapshot = telemetry.wait_for_fields(("state",), timeout=args.reply_timeout) or {}

    if "estop_held" in telemetry.error_codes() or snapshot.get("estop") == "1":
        _verdict(
            "E-STOP ENFONCE -- c'est votre blocage, et il n'est pas dans le code",
            "Le firmware refuse d'armer tant que l'E-stop est vu enfonce, et il a raison.\n"
            "\n"
            "Or AUCUN bouton n'est cable sur ce robot (safety_config.h). La broche est\n"
            "GPIO25 -- qui etait le PWMB du plan TB6612FNG abandonne. Un fil oublie de ce\n"
            "plan, reliant GPIO25 a la masse, se lit comme un bouton tenu enfonce en\n"
            "permanence : SAFE des la premiere boucle, tous les MOVE jetes, tous les\n"
            "resume refuses.\n"
            "\n"
            "A FAIRE : debrancher ce qui touche GPIO25, ou verifier au multimetre qu'elle\n"
            "est bien a 3V3 au repos. Puis relancer ce triage.",
        )
        return 1

    if snapshot.get("state") != "ACTIVE":
        _verdict(
            f"ARMEMENT REFUSE (etat={snapshot.get('state')})",
            "Le robot n'est pas passe en ACTIVE, donc motor_raw sera refuse et le test\n"
            "ne prouverait rien. Erreurs recues : "
            + (", ".join(c for c in telemetry.error_codes() if c) or "aucune")
            + "\n"
            "Rien n'a ete teste sur les moteurs.",
        )
        return 2

    print("[3/5] Remise a zero des compteurs d'encodeur ...")
    telemetry.forget("raw_ticks_left", "raw_ticks_right", "raw_edges_left", "raw_edges_right")
    link.send("SYSTEM", {"action": "reset_ticks"})
    time.sleep(0.3)
    link.send("SYSTEM", {"action": "raw_ticks"})
    before = telemetry.wait_for_fields(
        ("raw_ticks_left", "raw_ticks_right", "raw_edges_left", "raw_edges_right"),
        timeout=args.reply_timeout,
    )
    if before is None:
        _verdict(
            "FIRMWARE TROP ANCIEN",
            "Pas de reponse raw_ticks/raw_edges. Le firmware flashe est anterieur a\n"
            "celui de ce depot -- reflashez avant de conclure quoi que ce soit.",
        )
        return 2

    print(f"[4/5] Duty fixe {args.pwm} sur les deux moteurs pendant {args.duration}s (SANS PID) ...")
    link.send(
        "SYSTEM",
        {
            "action": "motor_raw",
            "left": str(args.pwm),
            "right": str(args.pwm),
            "ms": str(int(args.duration * 1000)),
        },
    )
    # Plus a margin: the firmware stops on its own at the end of the
    # window, and the counters must be read after it has.
    time.sleep(args.duration + 0.5)

    print("[5/5] Lecture des compteurs ...")
    telemetry.forget("raw_ticks_left", "raw_ticks_right", "raw_edges_left", "raw_edges_right")
    link.send("SYSTEM", {"action": "raw_ticks"})
    after = telemetry.wait_for_fields(
        ("raw_ticks_left", "raw_ticks_right", "raw_edges_left", "raw_edges_right"),
        timeout=args.reply_timeout,
    )
    if after is None:
        _verdict("PAS DE REPONSE", "Le robot a cesse de repondre pendant le test.")
        return 2

    return _report(before, after, telemetry, args)


def _report(
    before: dict[str, str],
    after: dict[str, str],
    telemetry: Telemetry,
    args: argparse.Namespace,
) -> int:
    def delta(key: str) -> int:
        try:
            return abs(int(after[key]) - int(before[key]))
        except (KeyError, ValueError):
            return 0

    ticks_left = delta("raw_ticks_left")
    ticks_right = delta("raw_ticks_right")
    edges_left = delta("raw_edges_left")
    edges_right = delta("raw_edges_right")
    edge_rate = max(edges_left, edges_right) / max(args.duration, 0.001)

    print(
        f"\n      gauche : {ticks_left:>7} ticks, {edges_left:>7} fronts"
        f"\n      droite : {ticks_right:>7} ticks, {edges_right:>7} fronts"
    )

    if "encoder_storm" in telemetry.error_codes() or edge_rate > IMPLAUSIBLE_EDGES_PER_S:
        _verdict(
            "ENTREE ENCODEUR QUI FLOTTE -- ce n'est pas un moteur mort",
            f"{edge_rate:.0f} fronts/s, physiquement impossible (ce chassis en produit ~320/s).\n"
            "\n"
            "GPIO34-39 n'ont aucun pull-up interne : un connecteur desserre laisse la\n"
            "broche flotter et l'interruption se declencher en continu, ce qui AFFAME la\n"
            "boucle qui pilote les moteurs. Les ticks ne le montrent pas, les +1/-1\n"
            "s'annulent -- c'est exactement pour ca que les fronts sont comptes a part.\n"
            "\n"
            "A FAIRE : reprendre le cablage de l'encodeur concerne, puis\n"
            "SYSTEM action=reset_ticks pour rearmer l'interruption.",
        )
        return 1

    if ticks_left == 0 and ticks_right == 0:
        _verdict(
            "ELECTRIQUE -- le logiciel est hors de cause",
            f"Duty {args.pwm} envoye directement au pont en H pendant {args.duration}s,\n"
            "sans PID ni encodeur dans la boucle : zero tick des deux cotes.\n"
            "\n"
            "Rien en amont du pont en H ne peut expliquer ca, et aucune correction\n"
            "firmware n'y changera quoi que ce soit. Meme conclusion que la bisection du\n"
            "2026-09-20 (le firmware entier de d9379a0 donnait deja ce resultat).\n"
            "\n"
            "A FAIRE, DANS CET ORDRE (bandeau de PROGRESS.md) :\n"
            "  1. Continuite GND ESP32 <-> GND DRV8833 au multimetre. Sans masse commune,\n"
            "     IN1/IN2 n'ont aucune reference : le driver ne commute pas et ne consomme\n"
            "     rien, alors que VM et SLEEP mesurent correctement. C'est exactement ce\n"
            "     qui est observe, et c'est le fil le plus manipule.\n"
            "  2. Tension aux bornes d'un moteur PENDANT qu'il force.\n"
            "  3. Courant debite par l'alim pendant la commande (0 A = le driver ne conduit\n"
            "     pas). Alim >= 3 A, verifier qu'elle ne passe pas en limitation.",
        )
        return 1

    if ticks_left == 0 or ticks_right == 0:
        dead = "gauche" if ticks_left == 0 else "droite"
        alive = "droite" if ticks_left == 0 else "gauche"
        _verdict(
            f"UN SEUL COTE REPOND (cote {dead} muet)",
            f"Le cote {alive} tourne, le cote {dead} ne compte rien. Le pont en H, l'alim\n"
            "et le firmware sont donc bons -- le probleme est sur cette voie-la.\n"
            "\n"
            "ATTENTION avant de conclure quel cote : moteur ET encodeur sont inverses\n"
            "ENSEMBLE dans le cablage (constate le 2026-09-20, la telemetrie comptait sur\n"
            "'gauche' pendant que la roue DROITE tournait). Le nom ci-dessus est celui du\n"
            "firmware, pas forcement celui de la roue que vous regardez.\n"
            "\n"
            "A FAIRE : verifier la voie muette (fils moteur, sorties du driver, connecteur\n"
            "encodeur), puis relancer. Un cote a la fois avec motor_raw left=255 right=0\n"
            "leve l'ambiguite gauche/droite en une mesure.",
        )
        return 1

    max_speed = _read_firmware_constant("ROVER_MAX_WHEEL_SPEED_MPS")
    ticks_per_rev = _read_firmware_constant("ROVER_ENCODER_TICKS_PER_REV")
    diameter = _read_firmware_constant("ROVER_WHEEL_DIAMETER_M")
    extra = ""
    if ticks_per_rev and diameter:
        import math

        slowest = min(ticks_left, ticks_right)
        mps = (slowest / ticks_per_rev) * (math.pi * diameter) / args.duration
        extra = f"\nVitesse mesuree a plein regime : {mps:.4f} m/s ({mps / (math.pi * diameter) * 60:.1f} tr/min).\n"
        if max_speed:
            ratio = max_speed / mps if mps > 0 else 0
            if ratio > 1.5 or ratio < 0.5:
                extra += (
                    f"ATTENTION : ROVER_MAX_WHEEL_SPEED_MPS vaut {max_speed:.3f}, soit {ratio:.1f}x cette\n"
                    f"mesure. Un plafond trop haut rend toute consigne inatteignable et colle le PWM\n"
                    f"a 255 ; trop bas, il bride le robot. Recalez-le sans reflasher :\n"
                    # .3f tronquait silencieusement toute vitesse < 0.001 m/s a
                    # "0.000" -- exactement le cas sur ce chassis (roues lentes,
                    # faible nombre de tours en args.duration) -- et
                    # recommandait donc une commande qui immobilise le robot.
                    f"  SYSTEM action=set_speed max={mps:.5f}\n"
                )
            else:
                extra += f"Coherent avec ROVER_MAX_WHEEL_SPEED_MPS ({max_speed:.3f}).\n"

    _verdict(
        "MOTEURS FONCTIONNELS -- le materiel repond",
        f"Les deux roues tournent sous duty fixe ({ticks_left} / {ticks_right} ticks).\n"
        "Pont en H, alim, moteurs et encodeurs sont donc bons.\n"
        f"{extra}"
        "\n"
        "Si le joystick ne faisait rien alors que CECI passe, la cause etait en amont du\n"
        "pont en H et elle est corrigee : etat SAFE invisible, reflexe d'obstacle\n"
        "silencieux, consigne inatteignable, rampe PID de plusieurs secondes.\n"
        "\n"
        "A FAIRE : reposer le robot, armer, et piloter. Commencez barre a fond -- un\n"
        "reglage bas demande un duty que ce chassis n'atteint pas.",
    )
    return 0


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Triage moteur : dit si la panne est electrique ou logicielle.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--port", required=True, help="/dev/ttyUSB0, COM10, socket://rover.lan:3333")
    parser.add_argument("--secret", default=None, help="ROVER_LINK_SECRET (liens reseau seulement)")
    parser.add_argument("--pwm", type=int, default=255, help="duty envoye au pont en H (defaut 255)")
    # Short by default and deliberately so: PROGRESS.md 2026-09-20 records
    # these motors degrading from one attempt to the next when pushed at
    # full duty (3125 ticks, then 116 at the same PWM half an hour later).
    # Work cold, in short bursts.
    parser.add_argument("--duration", type=float, default=3.0, help="duree du duty en s (defaut 3)")
    parser.add_argument("--connect-timeout", type=float, default=10.0)
    parser.add_argument("--reply-timeout", type=float, default=3.0)
    parser.add_argument("--yes", action="store_true", help="ne pas demander confirmation")
    args = parser.parse_args()

    print(
        f"\n  Ce test envoie un duty de {args.pwm} aux DEUX moteurs pendant {args.duration}s,\n"
        "  sans PID et sans asservissement. Le robot doit etre CALE, ROUES EN L'AIR.\n"
        "  Moteurs froids : ils se degradent d'essai en essai quand on insiste.\n"
    )
    if not args.yes:
        try:
            if input("  Roues en l'air ? [oui/non] ").strip().lower() not in ("oui", "o", "yes", "y"):
                print("  Annule.")
                sys.exit(3)
        except (EOFError, KeyboardInterrupt):
            print("\n  Annule.")
            sys.exit(3)

    sys.exit(_run(args))


if __name__ == "__main__":
    main()
