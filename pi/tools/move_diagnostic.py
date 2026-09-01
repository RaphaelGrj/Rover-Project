"""Non-interactive bring-up diagnostic: connects, optionally sends a timed
MOVE, and prints every frame received plus a boot-count summary.

Written during the 2026-09-01 motor bring-up session because
`tools/motor_test.py`'s interactive `cmd.Cmd` shell can't be driven from a
non-interactive tool session -- this one runs a fixed script instead, which
turned out to be exactly what's needed to spot reset loops (each boot logs
a fresh "SYSTEM ... state=BOOT" frame, so counting them tells you whether
the board is looping without needing to watch the raw serial log).

Examples (run from pi/):
  python -m tools.move_diagnostic --port COM10                     # idle, 6s, no MOVE
  python -m tools.move_diagnostic --port COM10 --move --duration 5 # drive for 5s
  python -m tools.move_diagnostic --port COM10 --move --velocity 0.05
"""
from __future__ import annotations

import argparse
import sys
import threading
import time

sys.path.insert(0, r"C:\Users\rapha\Rover-Project\pi")

from rover_esp32.link import RoverLink  # noqa: E402

HEARTBEAT_PERIOD_S = 0.15


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", required=True, help="e.g. COM10, /dev/ttyUSB0, rfc2217://localhost:4000")
    parser.add_argument("--duration", type=float, default=6.0, help="how long to listen/drive, seconds")
    parser.add_argument("--move", action="store_true", help="send HEARTBEAT+resume+MOVE instead of just listening")
    parser.add_argument("--velocity", type=float, default=0.15)
    parser.add_argument("--rotation", type=float, default=0.0)
    args = parser.parse_args()

    frames: list[tuple[str, dict]] = []
    link = RoverLink(args.port)
    link.on_frame = lambda t, f: frames.append((t, f))
    link.start()

    stop_hb = threading.Event()
    hb_thread: threading.Thread | None = None

    if args.move:
        def heartbeat_loop() -> None:
            while not stop_hb.is_set():
                link.send("HEARTBEAT")
                stop_hb.wait(HEARTBEAT_PERIOD_S)

        hb_thread = threading.Thread(target=heartbeat_loop, daemon=True)
        hb_thread.start()
        time.sleep(0.3)
        link.send("SYSTEM", {"action": "resume"})
        time.sleep(0.2)
        link.send("MOVE", {"velocity": str(args.velocity), "rotation": str(args.rotation)})
        time.sleep(args.duration)
        link.send("MOVE", {"velocity": "0", "rotation": "0"})
        time.sleep(0.3)
    else:
        time.sleep(args.duration)

    if hb_thread is not None:
        stop_hb.set()
    link.stop()

    boot_count = sum(1 for t, f in frames if t == "SYSTEM" and f.get("state") == "BOOT")
    print(f"--- {boot_count} boot(s) observed over {args.duration}s (move={args.move}) ---")
    for frame_type, fields in frames:
        print(f"{frame_type} " + " ".join(f"{k}={v}" for k, v in fields.items()))


if __name__ == "__main__":
    main()
