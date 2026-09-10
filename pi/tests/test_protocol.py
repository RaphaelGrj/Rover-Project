"""Tests for rover_esp32.protocol -- the Python side of Rover Protocol
(ROVER_PROTOCOL.md). Mirrors esp32/test/test_rover_protocol/ on the
firmware side: same checksum values, same edge cases, because these two
implementations must agree byte-for-byte (protocol.py's own docstring:
"if the two ever disagree, this is a protocol version bug").
"""
from __future__ import annotations

import pytest

from rover_esp32.protocol import FrameError, checksum, decode_frame, encode_frame


def test_checksum_matches_the_documented_example():
    # ROVER_PROTOCOL.md §3.1 / esp32/tools/rover_frame.py -- verified
    # independently during the Phase 1 Wokwi session (PROGRESS.md
    # 2026-08-25) after the doc's own first version of this example
    # turned out to be wrong.
    assert checksum("MOVE velocity=0.25 rotation=-0.10") == 0x39


def test_encode_frame_appends_checksum():
    assert encode_frame("MOVE", {"velocity": "0.25", "rotation": "-0.10"}) == \
        "MOVE velocity=0.25 rotation=-0.10 *39"


def test_encode_frame_with_no_fields():
    assert encode_frame("HEARTBEAT") == f"HEARTBEAT *{checksum('HEARTBEAT'):02X}"


def test_encode_frame_rejects_oversized_content():
    huge_fields = {"x": "a" * 200}
    with pytest.raises(FrameError):
        encode_frame("STATE", huge_fields)


def test_round_trip_encode_then_decode():
    line = encode_frame("MOVE", {"velocity": "0.25", "rotation": "-0.10"})
    frame_type, fields = decode_frame(line)
    assert frame_type == "MOVE"
    assert fields == {"velocity": "0.25", "rotation": "-0.10"}


def test_decode_frame_tolerates_crlf():
    line = encode_frame("HEARTBEAT")
    frame_type, fields = decode_frame(line + "\r\n")
    assert frame_type == "HEARTBEAT"
    assert fields == {}


def test_decode_frame_rejects_bad_checksum():
    with pytest.raises(FrameError):
        decode_frame("MOVE velocity=0.25 rotation=-0.10 *00")


def test_decode_frame_rejects_malformed_checksum_marker():
    with pytest.raises(FrameError):
        decode_frame("MOVE velocity=0.25")  # no " *CS" at all


def test_decode_frame_rejects_empty_line():
    with pytest.raises(FrameError):
        decode_frame("")


def test_decode_frame_rejects_non_ascii_content_instead_of_crashing():
    # Regression: a noisy serial line (electrical glitch) can carry bytes
    # outside ASCII in the content, before the checksum marker. This used
    # to raise UnicodeEncodeError out of checksum() instead of FrameError,
    # which escaped link.py's `except FrameError` and killed the pyserial
    # reader thread entirely (found 2026-09-10 while diagnosing a rover
    # that wouldn't move -- the diagnostic tool crashed on the very first
    # garbled line instead of dropping it and continuing).
    with pytest.raises(FrameError):
        decode_frame("MOVE vel\xe9ocity=0.25 rotation=-0.10 *00")


def test_checksum_rejects_non_ascii_content():
    with pytest.raises(FrameError):
        checksum("MOVE vel\xe9ocity=0.25")


def test_decode_frame_skips_malformed_field_silently():
    # A token with no "=" is dropped rather than erroring -- same
    # tolerance as the ESP32 parser (RoverProtocol.cpp).
    line = encode_frame("SYSTEM", {"action": "ping"})
    # Splice in a malformed extra token before the checksum marker.
    content, _, cs = line.rpartition(" *")
    tampered = f"{content} bogus *{checksum(content + ' bogus'):02X}"
    frame_type, fields = decode_frame(tampered)
    assert frame_type == "SYSTEM"
    assert fields == {"action": "ping"}
