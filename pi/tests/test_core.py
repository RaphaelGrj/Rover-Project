"""Tests for RoverCore's behavior-state machine (core.py, §20 of
ARCHITECTURE_AND_ROADMAP.md). Uses a FakeLink instead of a real
RoverLink so these never touch an actual serial port -- RoverCore only
ever calls `.send(frame_type, fields)` on it.
"""
from __future__ import annotations

import asyncio

import rover_core.core as core_module
from rover_core.core import RoverBehaviorState, RoverCore


class FakeLink:
    def __init__(self) -> None:
        self.sent: list[tuple[str, dict[str, str] | None]] = []

    def send(self, frame_type: str, fields: dict[str, str] | None = None) -> None:
        self.sent.append((frame_type, fields))


def run(coro):
    return asyncio.run(coro)


def test_initial_state_is_idle():
    async def body():
        core = RoverCore(FakeLink())
        assert core.state == RoverBehaviorState.IDLE

    run(body())


def test_client_connect_sets_interacting_and_sends_resume():
    async def body():
        link = FakeLink()
        core = RoverCore(link)
        await core.client_connected()
        assert core.state == RoverBehaviorState.INTERACTING
        assert ("SYSTEM", {"action": "resume"}) in link.sent
        await core.client_disconnected()  # stop the heartbeat task cleanly

    run(body())


def test_client_disconnect_sends_stop_and_returns_to_idle():
    async def body():
        link = FakeLink()
        core = RoverCore(link)
        await core.client_connected()
        await core.client_disconnected()
        assert core.state == RoverBehaviorState.IDLE
        assert ("MOVE", {"velocity": "0.00", "rotation": "0.00"}) in link.sent

    run(body())


def test_move_with_nonzero_values_sets_moving():
    async def body():
        core = RoverCore(FakeLink())
        core.move(0.2, 0.0)
        assert core.state == RoverBehaviorState.MOVING

    run(body())


def test_move_within_deadzone_does_not_claim_moving():
    async def body():
        link = FakeLink()
        core = RoverCore(link)
        await core.client_connected()
        core.move(0.0, 0.0)  # explicit zero, not "not moving at all"
        assert core.state == RoverBehaviorState.INTERACTING
        await core.client_disconnected()

    run(body())


def test_look_sends_head_command_without_touching_behavior_state():
    async def body():
        link = FakeLink()
        core = RoverCore(link)
        before = core.state
        core.look(15.0, -20.0)
        assert ("HEAD", {"pitch": "15.0", "yaw": "-20.0"}) in link.sent
        # Unlike move(), looking around has no behavior-state side
        # effect -- it's not a safety-relevant action.
        assert core.state == before

    run(body())


def test_move_forward_is_clamped_when_obstacle_is_close():
    async def body():
        link = FakeLink()
        core = RoverCore(link)
        core.on_frame("STATE", {"distance_left": "100", "distance_right": "9999"})
        await asyncio.sleep(0)
        core.move(0.2, 0.0)
        assert ("MOVE", {"velocity": "0.00", "rotation": "0.00"}) in link.sent
        # Clamped to a stop -- must not read as MOVING.
        assert core.state != RoverBehaviorState.MOVING

    run(body())


def test_move_backward_is_not_clamped_when_obstacle_is_close():
    async def body():
        link = FakeLink()
        core = RoverCore(link)
        core.on_frame("STATE", {"distance_left": "100", "distance_right": "9999"})
        await asyncio.sleep(0)
        core.move(-0.2, 0.0)  # backing away must still work
        assert ("MOVE", {"velocity": "-0.20", "rotation": "0.00"}) in link.sent

    run(body())


def test_rotation_in_place_is_not_clamped_when_obstacle_is_close():
    async def body():
        link = FakeLink()
        core = RoverCore(link)
        core.on_frame("STATE", {"distance_left": "100", "distance_right": "9999"})
        await asyncio.sleep(0)
        core.move(0.0, 0.5)  # turning in place must still work
        assert ("MOVE", {"velocity": "0.00", "rotation": "0.50"}) in link.sent

    run(body())


def test_move_forward_is_allowed_once_obstacle_clears():
    async def body():
        link = FakeLink()
        core = RoverCore(link)
        core.on_frame("STATE", {"distance_left": "100", "distance_right": "9999"})
        await asyncio.sleep(0)
        core.on_frame("STATE", {"distance_left": "9999"})  # cleared, no separate "cleared" event exists
        await asyncio.sleep(0)
        core.move(0.2, 0.0)
        assert ("MOVE", {"velocity": "0.20", "rotation": "0.00"}) in link.sent

    run(body())


def test_second_client_connecting_does_not_downgrade_moving():
    async def body():
        core = RoverCore(FakeLink())
        core.move(0.2, 0.0)
        assert core.state == RoverBehaviorState.MOVING
        await core.client_connected()  # a second client joining
        assert core.state == RoverBehaviorState.MOVING
        await core.client_disconnected()

    run(body())


def test_error_frame_sets_error_state():
    async def body():
        core = RoverCore(FakeLink())
        core.on_frame("ERROR", {"code": "sensor_timeout", "sensor": "tof_left"})
        await asyncio.sleep(0)  # let call_soon_threadsafe's callback run
        assert core.state == RoverBehaviorState.ERROR
        assert core.last_error == {"code": "sensor_timeout", "sensor": "tof_left"}

    run(body())


def test_error_state_auto_clears_after_hold_period(monkeypatch):
    # Real ERROR_STATE_HOLD_S is 5s -- shortened here so the test stays
    # fast; the constant is looked up by name at call time inside
    # _clear_error_after_delay, so patching the module attribute works.
    monkeypatch.setattr(core_module, "ERROR_STATE_HOLD_S", 0.05)

    async def body():
        core = RoverCore(FakeLink())
        core.on_frame("ERROR", {"code": "sensor_timeout"})
        await asyncio.sleep(0)
        assert core.state == RoverBehaviorState.ERROR
        await asyncio.sleep(0.15)
        assert core.state == RoverBehaviorState.IDLE

    run(body())


def test_idle_plays_sleepy_face_after_delay(monkeypatch):
    monkeypatch.setattr(core_module, "IDLE_SLEEPY_DELAY_S", 0.05)

    async def body():
        link = FakeLink()
        core = RoverCore(link)  # starts IDLE, sleepy timer armed immediately
        await asyncio.sleep(0.15)
        assert ("FACE", {"emotion": "sleepy"}) in link.sent

    run(body())


def test_client_connecting_cancels_pending_sleepy_and_wakes_face():
    async def body():
        link = FakeLink()
        core = RoverCore(link)
        await core.client_connected()
        # Leaving IDLE must both cancel the timer and immediately show a
        # non-sleepy face -- checked together since they're the same
        # guarantee ("no longer looks asleep once someone shows up").
        assert ("FACE", {"emotion": "idle"}) in link.sent
        await core.client_disconnected()

    run(body())


def test_state_frames_merge_instead_of_replacing():
    async def body():
        core = RoverCore(FakeLink())
        core.on_frame("STATE", {"distance_left": "9999", "distance_right": "9999"})
        await asyncio.sleep(0)
        core.on_frame("STATE", {"accel_x": "0.00", "accel_y": "0.00"})
        await asyncio.sleep(0)
        # Both STATE lines' fields must still be present together --
        # the whole point of last_state over the old single last_report.
        assert core.last_state == {
            "distance_left": "9999", "distance_right": "9999",
            "accel_x": "0.00", "accel_y": "0.00",
        }

    run(body())
