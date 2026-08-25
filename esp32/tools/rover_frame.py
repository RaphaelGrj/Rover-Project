#!/usr/bin/env python3
"""Compute a valid Rover Protocol frame (with checksum) to paste into
a serial monitor (Wokwi or real hardware) for manual testing.

Format (see ROVER_PROTOCOL.md section 3): "TYPE key=value ... *CS\n",
where CS is the XOR of every byte in "TYPE key=value ..." (no trailing
space, without the "*CS" itself), printed as 2 uppercase hex digits.
Doing this XOR by hand is error-prone, hence this script.

Usage:
    python3 rover_frame.py HEARTBEAT
    python3 rover_frame.py SYSTEM action=diag
    python3 rover_frame.py MOVE velocity=0.25 rotation=-0.10
"""
import sys


def build_frame(parts: list[str]) -> str:
    # Content = TYPE and key=value fields joined by a single space,
    # exactly as the ESP32 parser expects (RoverProtocol::handleLine).
    content = " ".join(parts)
    checksum = 0
    for byte in content.encode("ascii"):
        checksum ^= byte
    return f"{content} *{checksum:02X}"


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    print(build_frame(sys.argv[1:]))
