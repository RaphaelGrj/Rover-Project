"""aiohttp app: serves the control page and relays joystick/gamepad
input to RoverCore.move() over a WebSocket.

Single static HTML page, no JS framework, no build step
(ARCHITECTURE_AND_ROADMAP.md §27 rule 10, "ne pas ajouter une
dépendance lourde lorsqu'une solution simple suffit"). This module only
ever calls RoverCore's public methods -- it doesn't know or care that
there's a serial port and an ESP32 behind it.
"""
from __future__ import annotations

import json
import logging
from pathlib import Path
from typing import TYPE_CHECKING

from aiohttp import WSMsgType, web

if TYPE_CHECKING:
    from rover_core.core import RoverCore

logger = logging.getLogger(__name__)
STATIC_DIR = Path(__file__).parent / "static"


async def index(request: web.Request) -> web.FileResponse:
    return web.FileResponse(STATIC_DIR / "index.html")


def create_app(core: "RoverCore") -> web.Application:
    app = web.Application()
    app["core"] = core
    app.router.add_get("/", index)
    app.router.add_get("/ws", websocket_handler)
    return app


async def websocket_handler(request: web.Request) -> web.WebSocketResponse:
    ws = web.WebSocketResponse(heartbeat=10)
    await ws.prepare(request)
    core: "RoverCore" = request.app["core"]

    logger.info("control client connected (%s)", request.remote)
    await core.client_connected()

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
        logger.info("control client disconnected (%s)", request.remote)
        await core.client_disconnected()

    return ws
