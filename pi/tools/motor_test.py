"""Interactive serial console for bring-up testing of Phase 2 (motors/PID)
and generic diagnostics, without needing rover_core/rover_control running.

Run from pi/: `python -m tools.motor_test --port COM10` (or a /dev/ttyUSB*
path, or an rfc2217://... URL for Wokwi -- same RoverLink as the rest of
the Pi side, see rover_esp32/link.py).

Sends HEARTBEAT in the background (MOVE is gated on ACTIVE state and the
ESP32 drops to SAFE after ROVER_HEARTBEAT_TIMEOUT_MS without one -- see
ARCHITECTURE_AND_ROADMAP.md §9), and prints every STATE/EVENT/ERROR frame
as it arrives so left_speed/right_speed telemetry is visible live while
tuning.
"""
from __future__ import annotations

import argparse
import cmd
import threading
import time

from rover_esp32.link import RoverLink

# Matches rover_core.core.HEARTBEAT_PERIOD_S -- comfortably under the
# firmware's 500ms timeout.
HEARTBEAT_PERIOD_S = 0.15


class MotorTestShell(cmd.Cmd):
    intro = (
        "Rover motor/PID test console. Type `help` for commands, `quit` to exit.\n"
        "HEARTBEAT is sent automatically in the background; SYSTEM action=resume\n"
        "is sent once at startup so a SAFE ESP32 (e.g. after a fresh boot) becomes\n"
        "ACTIVE and accepts MOVE."
    )
    prompt = "rover> "

    def __init__(self, link: RoverLink) -> None:
        super().__init__()
        self._link = link

    def do_move(self, arg: str) -> None:
        """move VELOCITY ROTATION -- e.g. `move 0.20 0.0`"""
        try:
            velocity, rotation = (float(x) for x in arg.split())
        except ValueError:
            print("usage: move VELOCITY ROTATION")
            return
        self._link.send("MOVE", {"velocity": f"{velocity}", "rotation": f"{rotation}"})

    def do_stop(self, _arg: str) -> None:
        """stop -- shorthand for `move 0 0`"""
        self._link.send("MOVE", {"velocity": "0", "rotation": "0"})

    def do_diag(self, _arg: str) -> None:
        """diag -- ask the ESP32 for state/uptime/free_heap"""
        self._link.send("SYSTEM", {"action": "diag"})

    def do_get_pid(self, _arg: str) -> None:
        """get_pid -- print the currently active (possibly NVS-persisted) PID gains"""
        self._link.send("SYSTEM", {"action": "get_pid"})

    def do_set_pid(self, arg: str) -> None:
        """set_pid KP KI KD -- e.g. `set_pid 180 300 0` (persisted to NVS)"""
        try:
            kp, ki, kd = (float(x) for x in arg.split())
        except ValueError:
            print("usage: set_pid KP KI KD")
            return
        self._link.send("SYSTEM", {"action": "set_pid", "kp": f"{kp}", "ki": f"{ki}", "kd": f"{kd}"})

    def do_reset_pid(self, _arg: str) -> None:
        """reset_pid -- clear NVS-persisted gains, revert to compiled defaults"""
        self._link.send("SYSTEM", {"action": "reset_pid"})

    def do_resume(self, _arg: str) -> None:
        """resume -- SYSTEM action=resume (SAFE -> ACTIVE, e.g. after an obstacle/E-stop)"""
        self._link.send("SYSTEM", {"action": "resume"})

    def do_quit(self, _arg: str) -> bool:
        """quit -- stop the motors and exit"""
        self._link.send("MOVE", {"velocity": "0", "rotation": "0"})
        return True

    do_EOF = do_quit


def _print_frame(frame_type: str, fields: dict[str, str]) -> None:
    rendered = " ".join(f"{k}={v}" for k, v in fields.items())
    print(f"\n<- {frame_type} {rendered}\n{MotorTestShell.prompt}", end="", flush=True)


def _heartbeat_loop(link: RoverLink, stop: threading.Event) -> None:
    while not stop.is_set():
        link.send("HEARTBEAT")
        stop.wait(HEARTBEAT_PERIOD_S)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="e.g. COM10, /dev/ttyUSB0, rfc2217://localhost:4000")
    parser.add_argument("--baudrate", type=int, default=115200)
    args = parser.parse_args()

    link = RoverLink(args.port, baudrate=args.baudrate)
    link.on_frame = _print_frame
    link.start()

    stop_heartbeat = threading.Event()
    heartbeat_thread = threading.Thread(target=_heartbeat_loop, args=(link, stop_heartbeat), daemon=True)
    heartbeat_thread.start()

    # Give the ESP32 a moment to see the first HEARTBEATs before nudging
    # it out of SAFE -- resume is a no-op in ACTIVE, so no harm if it was
    # already there.
    time.sleep(0.3)
    link.send("SYSTEM", {"action": "resume"})

    try:
        MotorTestShell(link).cmdloop()
    finally:
        stop_heartbeat.set()
        link.stop()


if __name__ == "__main__":
    main()
