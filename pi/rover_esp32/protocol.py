"""Rover Protocol V1 wire format: encode/decode only, no I/O here.

Mirrors esp32/lib/communication/RoverProtocol.cpp byte-for-byte -- if the
two ever disagree, this is a protocol version bug (see ROVER_PROTOCOL.md
§29, "versionnement du protocole"), not a style choice to fix locally.
Frame: "TYPE key=value ... *CS\n", CS = XOR of every byte in "TYPE
key=value ..." (no trailing space, without "*CS" itself), 2 uppercase
hex digits. See ROVER_PROTOCOL.md §3.
"""
from __future__ import annotations

# Must match board_config.h's ROVER_MAX_FRAME_LEN.
MAX_FRAME_LEN = 128


class FrameError(ValueError):
    """A line couldn't be parsed as, or encoded into, a valid frame."""


def checksum(content: str) -> int:
    cs = 0
    for byte in content.encode("ascii"):
        cs ^= byte
    return cs


def encode_frame(frame_type: str, fields: dict[str, str] | None = None) -> str:
    """Returns "TYPE key=value ... *CS" (no trailing newline -- callers
    append that when writing to the serial port)."""
    parts = [frame_type]
    if fields:
        parts.extend(f"{key}={value}" for key, value in fields.items())
    content = " ".join(parts)
    if len(content) > MAX_FRAME_LEN:
        # Would just be rejected by the ESP32 as frame_too_long anyway;
        # fail here so the bug is caught at the sender instead of
        # silently losing a command.
        raise FrameError(f"encoded frame exceeds {MAX_FRAME_LEN} bytes: {content!r}")
    return f"{content} *{checksum(content):02X}"


def decode_frame(line: str) -> tuple[str, dict[str, str]]:
    """Parses one line (as received, before stripping) into
    (frame_type, fields). Raises FrameError on any format/checksum
    problem -- callers decide whether to log-and-drop or propagate."""
    line = line.rstrip("\r\n")
    if not line:
        raise FrameError("empty line")

    # Split "<content> *<CS>" at the last space, per ROVER_PROTOCOL.md §3.1.
    last_space = line.rfind(" ")
    if last_space == -1 or not line.startswith("*", last_space + 1) or len(line) - last_space - 2 != 2:
        raise FrameError(f"malformed checksum marker: {line!r}")

    cs_hex = line[last_space + 2:]
    try:
        received_cs = int(cs_hex, 16)
    except ValueError as exc:
        raise FrameError(f"invalid checksum hex {cs_hex!r}: {exc}") from exc

    content = line[:last_space]
    if checksum(content) != received_cs:
        raise FrameError(f"checksum mismatch for {line!r}")

    tokens = content.split(" ")
    frame_type = tokens[0]
    fields: dict[str, str] = {}
    for token in tokens[1:]:
        key, sep, value = token.partition("=")
        if sep:  # silently skip a malformed token with no '=', same as the ESP32 parser
            fields[key] = value
    return frame_type, fields
