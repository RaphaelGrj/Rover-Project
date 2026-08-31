"""Ties the ESP32 link to whatever is currently allowed to drive Rover.

Mirrors the ESP32's own safety principle instead of duplicating it: the
firmware already stops the motors if HEARTBEAT stops arriving for
ROVER_HEARTBEAT_TIMEOUT_MS (ROVER_PROTOCOL.md §6/§9, "la sécurité ne
doit jamais dépendre du Raspberry Pi"). So when no control client is
connected, RoverCore simply stops sending HEARTBEAT and lets that
hardware-level timeout do its job, rather than re-implementing a
second, Pi-side "should I stop the motors" decision that could disagree
with the ESP32's.
"""
from __future__ import annotations

import asyncio
import logging
from enum import Enum, auto
from typing import Callable

from rover_esp32.link import RoverLink

FrameListener = Callable[[str, dict[str, str]], None]

logger = logging.getLogger(__name__)

# Comfortably under ROVER_HEARTBEAT_TIMEOUT_MS (500ms, board_config.h) --
# a few missed beats from scheduling jitter shouldn't ever trip a SAFE
# while a client is actively connected.
HEARTBEAT_PERIOD_S = 0.15

# Below this, a MOVE's velocity/rotation counts as "not really moving"
# for behavior-state purposes (a client sending 0.00/0.00 while idle
# shouldn't read as MOVING).
MOVEMENT_DEADZONE = 0.02

# How long ERROR is held as the behavior state before falling back,
# absent a new ERROR -- mirrors the control UI's own ERROR line timeout
# (rover_control/static/index.html) so the two don't disagree about how
# "sticky" an error looks.
ERROR_STATE_HOLD_S = 5.0

# How long Rover stays IDLE (no control client connected) before
# playing FACE emotion=sleepy on its own -- purely cosmetic (the ESP32
# is already safely stopped via the heartbeat timeout well before this),
# just makes an unattended Rover look alive instead of frozen on
# whatever expression it last had.
IDLE_SLEEPY_DELAY_S = 120.0

# Mirrors ROVER_OBSTACLE_THRESHOLD_MM (esp32/include/sensors_config.h)
# and the same constant already duplicated client-side in
# rover_control/static/index.html -- distance_left/right below this is
# exactly what makes the ESP32 itself fire EVENT name=obstacle_detected,
# so this reflex agrees with what the firmware and the UI both consider
# "close".
OBSTACLE_THRESHOLD_MM = 150.0


class RoverBehaviorState(Enum):
    """Pi-side, high-level behavior state (ARCHITECTURE_AND_ROADMAP.md
    §20) -- deliberately NOT the ESP32's own hardware state machine
    (BOOT/READY/ACTIVE/SAFE/ERROR, board_config.h); the two "ne doivent
    pas être confondus" per that section. Most values below aren't
    reachable yet since the phases that would drive them don't exist
    yet (no vision, navigation, audio, or battery monitoring) -- they're
    listed now, matching §20's diagram exactly, so those future phases
    have a state to transition into without redesigning this enum.
    """
    BOOT = auto()
    IDLE = auto()
    INTERACTING = auto()
    MOVING = auto()
    EXPLORING = auto()   # Phase 9 (navigation autonome) -- unused today
    PATROLLING = auto()  # Phase 9 -- unused today
    FOLLOWING = auto()   # Phase 8 (vision) -- unused today
    CHARGING = auto()    # Phase 19 (énergie) -- unused today
    SLEEPING = auto()    # Phase 11 (comportement) -- unused today
    ERROR = auto()


class RoverCore:
    def __init__(self, link: RoverLink) -> None:
        self.link = link
        # STATE fields accumulate here (each STATE line only carries a
        # subset -- distance, IMU, environment, wheel speed, diag --
        # merging keeps the latest known value of every field ever seen,
        # instead of a late-joining client only seeing whichever single
        # STATE line happened to arrive last). EVENT/ERROR stay as one
        # discrete occurrence each, not merged -- see rover_control's UI.
        self.last_state: dict[str, str] = {}
        self.last_event: dict[str, str] | None = None
        self.last_error: dict[str, str] | None = None
        self.state = RoverBehaviorState.IDLE
        self._error_clear_task: asyncio.Task | None = None
        self._heartbeat_task: asyncio.Task | None = None
        # Multiple control clients (e.g. a phone and a laptop tab at
        # once) are allowed; only stop heartbeating once the *last* one
        # leaves, not the first.
        self._client_count = 0
        # Anyone wanting a live feed of STATE/EVENT/ERROR frames (the
        # control page's status panel, see rover_control.server)
        # subscribes here instead of RoverCore knowing anything about
        # WebSockets itself.
        self._listeners: set[FrameListener] = set()
        # Must be constructed from inside a running event loop (see
        # rover_core.main.async_main) so this captures the *actual* loop
        # aiohttp will be driving -- call_soon_threadsafe below is only
        # correct if it's the same loop the reader thread's callback
        # ends up scheduled on.
        self._loop = asyncio.get_running_loop()
        # Starts IDLE (above), so the sleepy timer must be armed here
        # too -- _set_state() only arms it on a *transition into* IDLE,
        # which this initial assignment isn't.
        self._idle_sleepy_task = asyncio.create_task(self._go_idle_sleepy_after_delay())

    # --- ESP32 -> Pi -------------------------------------------------

    def on_frame(self, frame_type: str, fields: dict[str, str]) -> None:
        """RoverLink callback -- runs on the serial reader thread, so
        this only ever does a thread-safe hop back onto the event loop;
        no shared state is touched here directly."""
        self._loop.call_soon_threadsafe(self._handle_frame, frame_type, fields)

    def _handle_frame(self, frame_type: str, fields: dict[str, str]) -> None:
        if frame_type == "STATE":
            self.last_state.update(fields)
        elif frame_type == "EVENT":
            self.last_event = dict(fields)
        elif frame_type == "ERROR":
            self.last_error = dict(fields)
        else:
            return

        logger.info("ESP32 -> %s %s", frame_type, fields)
        for listener in list(self._listeners):
            listener(frame_type, fields)

        if frame_type == "ERROR":
            # A real hardware error (ARCHITECTURE_AND_ROADMAP.md section
            # 21) takes priority over whatever the behavior state was --
            # held for ERROR_STATE_HOLD_S, then falls back to wherever
            # client-connection state alone would put it, unless another
            # ERROR arrives first and restarts the hold.
            self._set_state(RoverBehaviorState.ERROR)
            if self._error_clear_task is not None:
                self._error_clear_task.cancel()
            self._error_clear_task = asyncio.create_task(self._clear_error_after_delay())

    def _set_state(self, new_state: RoverBehaviorState) -> None:
        if new_state == self.state:
            return
        old_state = self.state
        self.state = new_state
        logger.info("behavior state -> %s", new_state.name)
        # Piggybacks on the same STATE/EVENT/ERROR listener mechanism as
        # a synthetic frame type, rather than a second parallel
        # notification path -- rover_control's UI already forwards
        # whatever it receives here generically (server.py).
        for listener in list(self._listeners):
            listener("ROVER_STATE", {"state": new_state.name})

        if old_state == RoverBehaviorState.IDLE:
            # Leaving IDLE cancels the pending sleepy timer, and wakes
            # the face back up in case it had actually gone sleepy --
            # harmless (just re-sends "idle") if it hadn't yet.
            if self._idle_sleepy_task is not None:
                self._idle_sleepy_task.cancel()
                self._idle_sleepy_task = None
            self.link.send("FACE", {"emotion": "idle"})
        if new_state == RoverBehaviorState.IDLE:
            self._idle_sleepy_task = asyncio.create_task(self._go_idle_sleepy_after_delay())

    async def _clear_error_after_delay(self) -> None:
        try:
            await asyncio.sleep(ERROR_STATE_HOLD_S)
            self._set_state(
                RoverBehaviorState.INTERACTING if self._client_count > 0 else RoverBehaviorState.IDLE
            )
        except asyncio.CancelledError:
            pass

    async def _go_idle_sleepy_after_delay(self) -> None:
        try:
            await asyncio.sleep(IDLE_SLEEPY_DELAY_S)
            self.link.send("FACE", {"emotion": "sleepy"})
        except asyncio.CancelledError:
            pass

    def add_listener(self, listener: FrameListener) -> None:
        self._listeners.add(listener)

    def remove_listener(self, listener: FrameListener) -> None:
        self._listeners.discard(listener)

    # --- Pi -> ESP32 ---------------------------------------------------

    def _obstacle_ahead(self) -> bool:
        """Re-evaluated from the latest STATE telemetry every time,
        rather than latched from the EVENT itself -- the firmware only
        ever sends EVENT name=obstacle_detected on the *rising* edge
        (ROVER_PROTOCOL.md §7.2), there is no corresponding "cleared"
        event, so treating that EVENT as a lasting flag here would leave
        the reflex stuck on forever after the first trigger. STATE
        distance_left/right arrives continuously regardless, so it's
        the only signal that can say "still close" vs. "clear now"."""
        try:
            left = float(self.last_state.get("distance_left", "inf"))
            right = float(self.last_state.get("distance_right", "inf"))
        except ValueError:
            return False
        return left < OBSTACLE_THRESHOLD_MM or right < OBSTACLE_THRESHOLD_MM

    def move(self, velocity: float, rotation: float) -> None:
        if velocity > 0.0 and self._obstacle_ahead():
            # Safety clamp, not navigation: refuses to drive *further*
            # into a detected obstacle (eg. a held joystick, a stuck
            # gamepad axis) -- backing away (negative velocity) and
            # turning in place both stay available, the Pi still decides
            # what to do next.
            velocity = 0.0
        self.link.send("MOVE", {"velocity": f"{velocity:.2f}", "rotation": f"{rotation:.2f}"})
        if abs(velocity) > MOVEMENT_DEADZONE or abs(rotation) > MOVEMENT_DEADZONE:
            self._set_state(RoverBehaviorState.MOVING)
        elif self._client_count > 0:
            self._set_state(RoverBehaviorState.INTERACTING)

    def look(self, pitch_deg: float, yaw_deg: float) -> None:
        """HEAD pitch/yaw (esp32/lib/head/HeadController.h) -- purely a
        pass-through, no behavior-state or safety logic of its own:
        unlike MOVE, looking around has no physical safety implication,
        the ESP32 already soft-limits and smooths it independently
        (head_config.h), and it's already gated on ACTIVE there too."""
        self.link.send("HEAD", {"pitch": f"{pitch_deg:.1f}", "yaw": f"{yaw_deg:.1f}"})

    async def client_connected(self) -> None:
        """A control client just took over. Explicitly resume (covers
        both "was already SAFE from a previous disconnect" and "first
        connection after boot, still READY") -- HEARTBEAT alone does not
        bring the ESP32 out of SAFE, only SYSTEM action=resume does
        (main.cpp), so skipping this would leave the robot refusing to
        move until someone resumes it by hand."""
        self._client_count += 1
        # Don't downgrade an already-MOVING state just because a second
        # client joined (eg. a phone connecting while a gamepad is
        # already driving) -- only claim INTERACTING if nothing more is
        # already happening.
        if self.state not in (RoverBehaviorState.MOVING, RoverBehaviorState.ERROR):
            self._set_state(RoverBehaviorState.INTERACTING)
        self.link.send("SYSTEM", {"action": "resume"})
        if self._heartbeat_task is None:
            self._heartbeat_task = asyncio.create_task(self._heartbeat_loop())

    async def client_disconnected(self) -> None:
        """One control client left. Only stop heartbeating once *no*
        client remains (a second client should keep driving unaffected
        if a first one disconnects) -- the ESP32 will then put itself in
        SAFE within ROVER_HEARTBEAT_TIMEOUT_MS on its own, plus one
        immediate stop sent here as a fast, best-effort extra. That MOVE
        is *not* load-bearing for safety: the hardware timeout already
        guarantees the motors stop even if this frame is lost."""
        self._client_count = max(0, self._client_count - 1)
        if self._client_count > 0:
            return
        if self._heartbeat_task is not None:
            self._heartbeat_task.cancel()
            self._heartbeat_task = None
        self.move(0.0, 0.0)
        if self.state != RoverBehaviorState.ERROR:
            self._set_state(RoverBehaviorState.IDLE)

    async def _heartbeat_loop(self) -> None:
        try:
            while True:
                self.link.send("HEARTBEAT")
                await asyncio.sleep(HEARTBEAT_PERIOD_S)
        except asyncio.CancelledError:
            pass
