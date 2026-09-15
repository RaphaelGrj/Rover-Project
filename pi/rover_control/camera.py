"""MJPEG video -- reverse-proxies the stream served by the ESP32-CAM
(a separate, deported camera module, see ARCHITECTURE_AND_ROADMAP.md
§4.3: "the ESP32-CAM films, the Pi analyzes") through this server's own
authenticated `/video` endpoint, so that clients reaching Rover through
this server (the control page, and anything outside the LAN via the
Phase 6 VPN) need a valid token to see the feed.

What this does NOT do, despite how it reads at first glance: it does not
*prevent* direct access to the camera. The ESP32-CAM serves its stream
unauthenticated on the local network (esp32-cam/src/main.cpp says so
explicitly) and stays reachable at its own address whether or not this
proxy exists -- anyone already on the WiFi can just open it. This
endpoint adds authentication for everyone coming through the Pi; it is
not a substitute for the camera having none of its own. Closing that
gap means putting the camera on an isolated network segment, or adding
a check to its firmware -- neither is done today.

"Hardware optional" -- same principle used throughout this project
(esp32/lib/sensors/I2CProbe.h is the equivalent idea on the firmware
side): no `camera_url` configured, or the ESP32-CAM unreachable when a
client asks for `/video`, both report `503 unavailable` rather than
crashing or taking down the rest of the control server.

MJPEG-over-HTTP (multipart/x-mixed-replace), not WebRTC: it needs no
signaling server, no STUN/TURN, and displays in the browser with a
plain <img> tag -- consistent with this project's "no heavy dependency
when a simple one will do" rule (ARCHITECTURE_AND_ROADMAP.md §27.10).
The exact multipart boundary is whatever the ESP32-CAM firmware used
(esp32-cam/src/main.cpp) -- this module mirrors its Content-Type
header verbatim rather than re-encoding frames itself.
"""
from __future__ import annotations

import asyncio
import logging

import aiohttp
from aiohttp import web

logger = logging.getLogger(__name__)

# Connect fast-fails rather than hanging the requesting browser tab if
# the ESP32-CAM is powered off/unreachable; sock_read is generous
# because a slow but alive stream (Pi under load, weak WiFi) shouldn't
# be mistaken for a dead one.
_CONNECT_TIMEOUT_S = 5
_READ_TIMEOUT_S = 15


class CameraStream:
    def __init__(self, camera_url: str | None = None) -> None:
        self._camera_url = camera_url or None
        self.available = self._camera_url is not None
        self._session: aiohttp.ClientSession | None = None
        if not self.available:
            logger.info("camera stream unavailable (no camera_url configured) -- /video will report unavailable")

    async def _get_session(self) -> aiohttp.ClientSession:
        # Created lazily, inside the running event loop that will
        # actually use it (aiohttp.ClientSession is unhappy about being
        # constructed outside one) -- and reused across requests rather
        # than opening a fresh TCP connection to the ESP32-CAM every
        # time someone (re)loads the control page.
        if self._session is None or self._session.closed:
            self._session = aiohttp.ClientSession(
                timeout=aiohttp.ClientTimeout(connect=_CONNECT_TIMEOUT_S, sock_read=_READ_TIMEOUT_S)
            )
        return self._session

    async def close(self) -> None:
        if self._session is not None and not self._session.closed:
            await self._session.close()

    async def mjpeg_response(self, request: web.Request) -> web.StreamResponse:
        if not self.available:
            return web.Response(status=503, text="camera unavailable")

        session = await self._get_session()
        try:
            upstream = await session.get(self._camera_url)
        except (aiohttp.ClientError, asyncio.TimeoutError) as exc:
            logger.warning("ESP32-CAM unreachable at %s: %s", self._camera_url, exc)
            return web.Response(status=503, text="camera unavailable")

        if upstream.status != 200:
            upstream.release()
            logger.warning("ESP32-CAM returned HTTP %d for %s", upstream.status, self._camera_url)
            return web.Response(status=503, text="camera unavailable")

        # Mirrored, not hardcoded: the real boundary lives in the
        # ESP32-CAM firmware (esp32-cam/src/main.cpp) -- duplicating it
        # here would be one more place to keep in sync if it ever
        # changes.
        content_type = upstream.headers.get("Content-Type", "multipart/x-mixed-replace")
        response = web.StreamResponse(status=200, headers={"Content-Type": content_type})
        await response.prepare(request)
        try:
            async for chunk in upstream.content.iter_any():
                await response.write(chunk)
        except (ConnectionResetError, asyncio.CancelledError):
            pass  # client navigated away / disconnected -- not an error
        finally:
            upstream.release()
        return response
