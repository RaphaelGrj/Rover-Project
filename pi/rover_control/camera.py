"""MJPEG camera stream -- entirely optional, the same "hardware
optional" principle used throughout this project (see
esp32/lib/sensors/I2CProbe.h for the equivalent idea on the firmware
side). No camera exists on Rover yet (BOM.md: Phase 6 is still "hors
périmètre") -- this gives the control page somewhere to show a feed
from once one does, without anything crashing or breaking in the
meantime.

Uses picamera2 (the current, actively-maintained Raspberry Pi camera
stack) if it's importable and a camera is actually present; otherwise
`available` stays False and /video reports that plainly rather than
taking down the rest of the control server over a missing camera.

MJPEG-over-HTTP (multipart/x-mixed-replace), not WebRTC: it needs no
signaling server, no STUN/TURN, and displays in the browser with a
plain <img> tag -- consistent with this project's "no heavy dependency
when a simple one will do" rule (ARCHITECTURE_AND_ROADMAP.md §27.10).
"""
from __future__ import annotations

import asyncio
import io
import logging

from aiohttp import web

logger = logging.getLogger(__name__)

_BOUNDARY = "roverframe"
# ~10 fps: plenty for a control-page situational preview, not meant to
# be a low-latency FPV feed.
_FRAME_INTERVAL_S = 0.1


class CameraStream:
    def __init__(self) -> None:
        self._picam2 = None
        self.available = False
        try:
            from picamera2 import Picamera2  # type: ignore[import-not-found]

            self._picam2 = Picamera2()
            self._picam2.configure(self._picam2.create_video_configuration(main={"size": (640, 480)}))
            self._picam2.start()
            self.available = True
            logger.info("camera stream available (picamera2)")
        except Exception as exc:  # noqa: BLE001 - deliberately broad, see module docstring
            # Covers picamera2 not installed (ImportError -- the normal
            # case on any machine that isn't the Pi itself, including
            # this dev machine), no camera ribbon attached, permission
            # issues, or the wrong platform entirely.
            logger.info("camera stream unavailable (%s) -- /video will report unavailable", exc)

    def _capture_jpeg(self) -> bytes:
        stream = io.BytesIO()
        self._picam2.capture_file(stream, format="jpeg")
        return stream.getvalue()

    async def mjpeg_response(self, request: web.Request) -> web.StreamResponse:
        if not self.available:
            return web.Response(status=503, text="camera unavailable")

        response = web.StreamResponse(
            status=200,
            headers={"Content-Type": f"multipart/x-mixed-replace; boundary={_BOUNDARY}"},
        )
        await response.prepare(request)
        try:
            while True:
                # capture_file() blocks on hardware I/O -- run it off the
                # event loop so one video client can't stall everything
                # else this server does (WebSocket control commands,
                # other clients).
                frame = await asyncio.to_thread(self._capture_jpeg)
                header = f"--{_BOUNDARY}\r\nContent-Type: image/jpeg\r\nContent-Length: {len(frame)}\r\n\r\n"
                await response.write(header.encode("ascii") + frame + b"\r\n")
                await asyncio.sleep(_FRAME_INTERVAL_S)
        except (ConnectionResetError, asyncio.CancelledError):
            pass  # client navigated away / disconnected -- not an error
        return response
