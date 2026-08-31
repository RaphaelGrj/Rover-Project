"""aiohttp app: serves the control page and relays joystick/gamepad
input to RoverCore.move() over a WebSocket.

Single static HTML page, no JS framework, no build step
(ARCHITECTURE_AND_ROADMAP.md §27 rule 10, "ne pas ajouter une
dépendance lourde lorsqu'une solution simple suffit"). This module only
ever calls RoverCore's public methods -- it doesn't know or care that
there's a serial port and an ESP32 behind it.
"""
from __future__ import annotations

import asyncio
import json
import logging
from pathlib import Path
from typing import TYPE_CHECKING

from aiohttp import WSMsgType, web

from .auth import token_matches
from .camera import CameraStream

if TYPE_CHECKING:
    from rover_core.core import RoverCore

logger = logging.getLogger(__name__)
STATIC_DIR = Path(__file__).parent / "static"


async def index(request: web.Request) -> web.FileResponse:
    return web.FileResponse(STATIC_DIR / "index.html")


@web.middleware
async def auth_middleware(request: web.Request, handler):
    """Applies to every route on this app (see create_app) -- a new
    route added later (the /video endpoint added this same session is
    exactly the case this guards against) is protected automatically
    instead of relying on remembering to add the check to it too."""
    token = request.app["token"]
    if not token_matches(request.query.get("token"), token):
        return web.Response(status=403, text="Forbidden: missing or invalid ?token=")
    return await handler(request)


def create_app(core: "RoverCore", token: str, camera: CameraStream | None = None) -> web.Application:
    app = web.Application(middlewares=[auth_middleware])
    app["core"] = core
    app["token"] = token
    app["camera"] = camera or CameraStream()
    app.router.add_get("/", index)
    app.router.add_get("/ws", websocket_handler)
    app.router.add_get("/video", video_handler)
    return app


async def video_handler(request: web.Request) -> web.StreamResponse:
    camera: CameraStream = request.app["camera"]
    return await camera.mjpeg_response(request)


async def websocket_handler(request: web.Request) -> web.WebSocketResponse:
    ws = web.WebSocketResponse(heartbeat=10)
    await ws.prepare(request)
    core: "RoverCore" = request.app["core"]

    def on_esp32_frame(frame_type: str, fields: dict[str, str]) -> None:
        # RoverCore calls this synchronously from within the same loop
        # (see core._handle_frame), so we're already on the right event
        # loop here -- just can't await directly from a plain callback,
        # hence scheduling the actual send as its own task. A client
        # that closed between frames makes send_json raise; that's
        # expected and not worth logging.
        async def _send() -> None:
            try:
                await ws.send_json({"type": frame_type, **fields})
            except ConnectionResetError:
                pass

        asyncio.create_task(_send())

    core.add_listener(on_esp32_frame)
    logger.info("control client connected (%s)", request.remote)
    await core.client_connected()

    # A client connecting mid-session shouldn't see a blank status panel
    # until the next frame happens to arrive from the ESP32 -- replay
    # everything currently known: the merged STATE fields (distance/IMU/
    # environment/wheel speed all accumulate into one dict, see
    # RoverCore._handle_frame), the last EVENT/ERROR if any, and the
    # current high-level behavior state.
    if core.last_state:
        await ws.send_json({"type": "STATE", **core.last_state})
    if core.last_event:
        await ws.send_json({"type": "EVENT", **core.last_event})
    if core.last_error:
        await ws.send_json({"type": "ERROR", **core.last_error})
    await ws.send_json({"type": "ROVER_STATE", "state": core.state.name})

    try:
        async for msg in ws:
            if msg.type != WSMsgType.TEXT:
                continue
            try:
                data = json.loads(msg.data)
                velocity = float(data["velocity"])
                rotation = float(data["rotation"])
            except (ValueError, KeyError, TypeError, json.JSONDecodeError):
                logger.warning("ignoring malformed control message: %r", msg.data)
                continue
            core.move(velocity, rotation)
    finally:
        core.remove_listener(on_esp32_frame)
        logger.info("control client disconnected (%s)", request.remote)
        await core.client_disconnected()

    return ws
