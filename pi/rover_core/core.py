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

from rover_esp32.link import RoverLink

logger = logging.getLogger(__name__)

# Comfortably under ROVER_HEARTBEAT_TIMEOUT_MS (500ms, board_config.h) --
# a few missed beats from scheduling jitter shouldn't ever trip a SAFE
# while a client is actively connected.
HEARTBEAT_PERIOD_S = 0.15


class RoverCore:
    def __init__(self, link: RoverLink) -> None:
        self.link = link
        self.last_report: dict[str, str] = {}
        self._heartbeat_task: asyncio.Task | None = None
        # Multiple control clients (e.g. a phone and a laptop tab at
        # once) are allowed; only stop heartbeating once the *last* one
        # leaves, not the first.
        self._client_count = 0
        # Must be constructed from inside a running event loop (see
        # rover_core.main.async_main) so this captures the *actual* loop
        # aiohttp will be driving -- call_soon_threadsafe below is only
        # correct if it's the same loop the reader thread's callback
        # ends up scheduled on.
        self._loop = asyncio.get_running_loop()

    # --- ESP32 -> Pi -------------------------------------------------

    def on_frame(self, frame_type: str, fields: dict[str, str]) -> None:
        """RoverLink callback -- runs on the serial reader thread, so
        this only ever does a thread-safe hop back onto the event loop;
        no shared state is touched here directly."""
        self._loop.call_soon_threadsafe(self._handle_frame, frame_type, fields)

    def _handle_frame(self, frame_type: str, fields: dict[str, str]) -> None:
        if frame_type in ("STATE", "EVENT", "ERROR"):
            self.last_report = {"type": frame_type, **fields}
            logger.info("ESP32 -> %s %s", frame_type, fields)

    # --- Pi -> ESP32 ---------------------------------------------------

    def move(self, velocity: float, rotation: float) -> None:
        self.link.send("MOVE", {"velocity": f"{velocity:.2f}", "rotation": f"{rotation:.2f}"})

    async def client_connected(self) -> None:
        """A control client just took over. Explicitly resume (covers
        both "was already SAFE from a previous disconnect" and "first
        connection after boot, still READY") -- HEARTBEAT alone does not
        bring the ESP32 out of SAFE, only SYSTEM action=resume does
        (main.cpp), so skipping this would leave the robot refusing to
        move until someone resumes it by hand."""
        self._client_count += 1
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

    async def _heartbeat_loop(self) -> None:
        try:
            while True:
                self.link.send("HEARTBEAT")
                await asyncio.sleep(HEARTBEAT_PERIOD_S)
        except asyncio.CancelledError:
            pass
