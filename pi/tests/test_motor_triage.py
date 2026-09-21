"""Tests for the motor triage tool's decision tree (tools/motor_triage.py).

The verdict IS the product here -- an operator acts on it, and on this
project acting on a wrong one costs a session. So the branches are
pinned: given a set of counters, which conclusion comes out.

Only `_report` is exercised, which is pure given its inputs. Everything
around it (link, heartbeat, command sequencing) is covered end to end by
running the tool against a simulated robot, which needs no test file.
"""
from __future__ import annotations

import argparse

from tools.motor_triage import _report
from tools import motor_triage


class FakeTelemetry:
    def __init__(self, codes: list[str] | None = None) -> None:
        self._codes = codes or []

    def error_codes(self) -> list[str]:
        return list(self._codes)


def _args(duration: float = 2.0, pwm: int = 255) -> argparse.Namespace:
    return argparse.Namespace(duration=duration, pwm=pwm)


def _counters(left_ticks: int, right_ticks: int, left_edges: int, right_edges: int):
    before = {
        "raw_ticks_left": "0", "raw_ticks_right": "0",
        "raw_edges_left": "0", "raw_edges_right": "0",
    }
    after = {
        "raw_ticks_left": str(left_ticks), "raw_ticks_right": str(right_ticks),
        "raw_edges_left": str(left_edges), "raw_edges_right": str(right_edges),
    }
    return before, after


# Exit codes are part of the contract: 0 = hardware answers, 1 = a fault
# was identified, 2 = inconclusive (handled in _triage, not here).

def test_no_ticks_at_all_is_called_electrical():
    """The decisive case. motor_raw bypasses the PID, the feed-forward,
    the speed ceiling and the obstacle reflex, so zero ticks under it
    leaves nothing upstream of the H-bridge to blame."""
    before, after = _counters(0, 0, 0, 0)
    assert _report(before, after, FakeTelemetry(), _args()) == 1


def test_both_wheels_turning_clears_the_hardware():
    before, after = _counters(640, 640, 640, 640)
    assert _report(before, after, FakeTelemetry(), _args()) == 0


def test_one_silent_side_is_not_reported_as_electrical():
    """A wheel that turns proves the bridge, the supply and the firmware
    are fine -- so this must never land on the 'check the common ground'
    verdict, which would send the operator after the wrong thing."""
    before, after = _counters(0, 640, 0, 640)
    assert _report(before, after, FakeTelemetry(), _args()) == 1


def test_an_edge_storm_outranks_the_electrical_verdict():
    """Ticks stay at zero during a storm (the +1/-1 decisions cancel), so
    it looks exactly like dead motors on the tick count alone. The edge
    rate has to win, or the operator is sent to the multimeter for a
    loose encoder connector."""
    edges = int(motor_triage.IMPLAUSIBLE_EDGES_PER_S * 2.0 * 2)  # 2x the limit over 2s
    before, after = _counters(0, 0, edges, 0)
    assert _report(before, after, FakeTelemetry(), _args(duration=2.0)) == 1


def test_a_reported_encoder_storm_is_believed_without_recomputing():
    """The firmware detaches the interrupt itself once it sees the storm
    (Encoder::pollStorm), which caps the edge count -- so the rate this
    tool computes afterwards can look innocent. The ERROR frame is the
    stronger signal and must be honoured on its own."""
    before, after = _counters(0, 0, 40, 0)
    assert _report(before, after, FakeTelemetry(["encoder_storm"]), _args()) == 1


def test_counters_that_went_backwards_are_still_measured():
    """Tick counters are signed and the sign depends on wiring that has
    been swapped more than once on this robot -- a wheel turning 'the
    wrong way' is still a wheel turning, and must not read as dead."""
    before = {
        "raw_ticks_left": "0", "raw_ticks_right": "0",
        "raw_edges_left": "0", "raw_edges_right": "0",
    }
    after = {
        "raw_ticks_left": "-640", "raw_ticks_right": "-640",
        "raw_edges_left": "640", "raw_edges_right": "640",
    }
    assert _report(before, after, FakeTelemetry(), _args()) == 0


def test_firmware_constants_are_read_from_the_header_not_duplicated():
    """This whole hunt began with a speed constant that was wrong by 10x.
    The tool reads the real one so it cannot disagree with the firmware
    about what it just measured."""
    assert motor_triage._read_firmware_constant("ROVER_ENCODER_TICKS_PER_REV") == 1073.0
    assert motor_triage._read_firmware_constant("ROVER_MAX_WHEEL_SPEED_MPS") is not None
    assert motor_triage._read_firmware_constant("NOT_A_REAL_CONSTANT") is None
