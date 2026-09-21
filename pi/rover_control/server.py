"""aiohttp app: serves the control page and relays joystick/gamepad
input to RoverCore.move()/look() over a WebSocket.

Single static HTML page, no JS framework, no build step
(ARCHITECTURE_AND_ROADMAP.md §27 rule 10, "ne pas ajouter une
dépendance lourde lorsqu'une solution simple suffit"). This module only
ever calls RoverCore's public methods -- it doesn't know or care that
there's a serial port and an ESP32 behind it.
"""
from __future__ import annotations

import asyncio
import base64
import json
import logging
from pathlib import Path
from typing import TYPE_CHECKING

from aiohttp import WSMsgType, web

from rover_ai import AIProviderError
from rover_audio import AudioProviderError

from .ai_panel import AIPanel
from .audio_panel import AudioPanel
from .auth import token_matches
from .camera import CameraStream
from .voice_panel import VoicePanel, VoiceTurnError

if TYPE_CHECKING:
    from rover_core.core import RoverCore

logger = logging.getLogger(__name__)
STATIC_DIR = Path(__file__).parent / "static"


async def index(request: web.Request) -> web.FileResponse:
    return web.FileResponse(STATIC_DIR / "index.html")


async def ai_page(request: web.Request) -> web.FileResponse:
    return web.FileResponse(STATIC_DIR / "ai.html")


async def ar_page(request: web.Request) -> web.FileResponse:
    return web.FileResponse(STATIC_DIR / "ar-hud.html")


async def audio_page(request: web.Request) -> web.FileResponse:
    return web.FileResponse(STATIC_DIR / "audio.html")


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


def create_app(
    core: "RoverCore",
    token: str,
    camera: CameraStream | None = None,
    ai_panel: AIPanel | None = None,
    audio_panel: AudioPanel | None = None,
    voice_panel: VoicePanel | None = None,
) -> web.Application:
    app = web.Application(middlewares=[auth_middleware])
    app["core"] = core
    app["token"] = token
    app["camera"] = camera or CameraStream()
    app["ai_panel"] = ai_panel or AIPanel()
    app["audio_panel"] = audio_panel or AudioPanel()
    app["voice_panel"] = voice_panel or VoicePanel(app["audio_panel"], app["ai_panel"])
    app.router.add_get("/", index)
    app.router.add_get("/ws", websocket_handler)
    app.router.add_get("/video", video_handler)
    app.router.add_get("/ai", ai_page)
    app.router.add_get("/ar", ar_page)
    app.router.add_get("/audio", audio_page)
    app.router.add_get("/ai/config", ai_config_get)
    app.router.add_post("/ai/config", ai_config_post)
    app.router.add_post("/ai/ask", ai_ask_post)
    app.router.add_post("/ai/reset", ai_reset_post)
    app.router.add_get("/audio/config", audio_config_get)
    app.router.add_post("/audio/config", audio_config_post)
    app.router.add_post("/audio/transcribe", audio_transcribe_post)
    app.router.add_post("/audio/speak", audio_speak_post)
    app.router.add_post("/audio/converse", audio_converse_post)
    return app


async def video_handler(request: web.Request) -> web.StreamResponse:
    camera: CameraStream = request.app["camera"]
    return await camera.mjpeg_response(request)


async def ai_config_get(request: web.Request) -> web.Response:
    panel: AIPanel = request.app["ai_panel"]
    return web.json_response(panel.public_config())


async def ai_config_post(request: web.Request) -> web.Response:
    panel: AIPanel = request.app["ai_panel"]
    try:
        raw = await request.json()
    except ValueError:
        return web.json_response({"error": "invalid JSON body"}, status=400)
    if not isinstance(raw, dict):
        return web.json_response({"error": "expected a JSON object"}, status=400)

    try:
        await panel.update(raw)
    except ValueError as exc:
        # save_credentials() rejects an unknown key -- can't actually
        # happen through this route (AIPanel.update() already filters
        # to CONFIG_FIELDS first), kept as a guard rather than trusting
        # that filtering silently forever.
        return web.json_response({"error": str(exc)}, status=400)
    return web.json_response(panel.public_config())


async def ai_ask_post(request: web.Request) -> web.Response:
    panel: AIPanel = request.app["ai_panel"]
    try:
        raw = await request.json()
        message = str(raw["message"])
    except (ValueError, TypeError, KeyError):
        return web.json_response({"error": 'expected a JSON body {"message": "..."}'}, status=400)

    try:
        reply = await panel.ask(message)
    except AIProviderError as exc:
        # Mirrors camera.py's "hardware/feature optional" 503, not a
        # 500 -- an unconfigured or unreachable AI provider is an
        # expected, recoverable state, not a server bug.
        return web.json_response({"error": str(exc)}, status=503)
    return web.json_response({"reply": reply})


async def ai_reset_post(request: web.Request) -> web.Response:
    """Starts a fresh conversation (drops the PersonalityEngine's
    history) -- the provider config/credentials are untouched."""
    panel: AIPanel = request.app["ai_panel"]
    panel.reset_conversation()
    return web.json_response({"ok": True})


async def audio_config_get(request: web.Request) -> web.Response:
    panel: AudioPanel = request.app["audio_panel"]
    return web.json_response(panel.public_config())


async def audio_config_post(request: web.Request) -> web.Response:
    panel: AudioPanel = request.app["audio_panel"]
    try:
        raw = await request.json()
    except ValueError:
        return web.json_response({"error": "invalid JSON body"}, status=400)
    if not isinstance(raw, dict):
        return web.json_response({"error": "expected a JSON object"}, status=400)

    try:
        await panel.update(raw)
    except ValueError as exc:
        return web.json_response({"error": str(exc)}, status=400)
    return web.json_response(panel.public_config())


async def audio_transcribe_post(request: web.Request) -> web.Response:
    """Body is the raw audio bytes (not JSON) -- a plain `fetch(url,
    {body: file})` from audio.html sends a File/Blob straight through,
    no multipart wrapping needed for a single clip."""
    panel: AudioPanel = request.app["audio_panel"]
    audio = await request.read()
    if not audio:
        return web.json_response({"error": "empty request body -- expected raw audio bytes"}, status=400)
    mime_type = request.content_type or "audio/wav"

    try:
        text = await panel.transcribe(audio, mime_type=mime_type)
    except AudioProviderError as exc:
        return web.json_response({"error": str(exc)}, status=503)
    return web.json_response({"text": text})


async def audio_speak_post(request: web.Request) -> web.Response:
    panel: AudioPanel = request.app["audio_panel"]
    try:
        raw = await request.json()
        text = str(raw["text"])
    except (ValueError, TypeError, KeyError):
        return web.json_response({"error": 'expected a JSON body {"text": "..."}'}, status=400)

    try:
        audio = await panel.synthesize(text)
    except AudioProviderError as exc:
        return web.json_response({"error": str(exc)}, status=503)
    # audio/mpeg matches what OpenAITTS actually returns (MP3); a local
    # provider that returns a different encoding (eg. WAV) would be
    # mislabeled here -- rover_audio.tts.TextToSpeechProvider doesn't
    # report its own content type yet (see PROGRESS.md), good enough for
    # this first increment where OpenAI is the only wired-up option.
    return web.Response(body=audio, content_type="audio/mpeg")


async def audio_converse_post(request: web.Request) -> web.Response:
    """One full voice turn: STT -> rover-ai (persona + history, §17.1)
    -> TTS (voice_panel.VoicePanel). Body is raw audio bytes, same shape
    as /audio/transcribe. Reply audio comes back base64-encoded inside
    JSON (alongside the heard/reply text) rather than as a raw binary
    response -- simpler and safer than smuggling non-ASCII reply text
    into HTTP headers, and the client needs the text anyway to display
    the exchange."""
    voice: VoicePanel = request.app["voice_panel"]
    audio = await request.read()
    if not audio:
        return web.json_response({"error": "empty request body -- expected raw audio bytes"}, status=400)
    mime_type = request.content_type or "audio/wav"

    try:
        heard_text, reply_text, reply_audio = await voice.converse(audio, mime_type=mime_type)
    except VoiceTurnError as exc:
        return web.json_response({"error": str(exc), "stage": exc.stage}, status=503)

    return web.json_response({
        "heard_text": heard_text,
        "reply_text": reply_text,
        "reply_audio_base64": base64.b64encode(reply_audio).decode("ascii"),
    })


async def websocket_handler(request: web.Request) -> web.WebSocketResponse:
    ws = web.WebSocketResponse(heartbeat=10)
    await ws.prepare(request)
    core: "RoverCore" = request.app["core"]

    # Frames are queued and drained by one dedicated task rather than
    # spawning a task per frame. Two reasons, both real:
    #  - asyncio only holds a weak reference to a bare create_task()
    #    result, so a task nobody keeps a reference to can be garbage
    #    collected mid-flight and silently never deliver its frame;
    #  - N concurrent send tasks have no defined completion order, so
    #    telemetry could reach the browser out of order -- which for a
    #    distance/state feed means showing a stale reading as current.
    # maxsize bounds memory if a client stops reading (a phone that went
    # to sleep, say): the oldest frames are dropped instead of growing
    # the queue without limit. Dropping telemetry is safe -- every STATE
    # field is re-sent periodically by the ESP32.
    outgoing: asyncio.Queue[dict] = asyncio.Queue(maxsize=256)

    def on_esp32_frame(frame_type: str, fields: dict[str, str]) -> None:
        # RoverCore calls this synchronously from within the same loop
        # (see core._handle_frame), so we're already on the right event
        # loop -- but we still can't await from a plain callback.
        try:
            outgoing.put_nowait({"type": frame_type, **fields})
        except asyncio.QueueFull:
            logger.warning("control client too slow, dropping a %s frame", frame_type)

    async def _pump() -> None:
        while True:
            message = await outgoing.get()
            try:
                await ws.send_json(message)
            except (ConnectionResetError, RuntimeError):
                # Client closed between frames -- expected, and the
                # `async for` loop below is what actually notices and
                # tears the connection down.
                return

    pump_task = asyncio.create_task(_pump())

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
            except json.JSONDecodeError:
                logger.warning("ignoring malformed control message: %r", msg.data)
                continue

            # Explicit re-arm from the page's "Activer" button. Handled
            # before the drive fields and then `continue`d: it is a
            # command of its own, not a MOVE carrying an extra key, and
            # an ESP32 in SAFE would discard any MOVE sent with it
            # anyway (main.cpp).
            if isinstance(data, dict) and data.get("action") == "resume":
                logger.info("control client requested a resume (%s)", request.remote)
                core.resume()
                continue

            try:
                velocity = float(data["velocity"])
                rotation = float(data["rotation"])
            except (ValueError, KeyError, TypeError):
                logger.warning("ignoring malformed control message: %r", msg.data)
                continue
            core.move(velocity, rotation)

            # head_pitch/head_yaw are optional -- older clients (or a
            # deliberately move-only one) that never send them just
            # never move the head, rather than being rejected outright.
            if "head_pitch" in data or "head_yaw" in data:
                try:
                    head_pitch = float(data.get("head_pitch", 0.0))
                    head_yaw = float(data.get("head_yaw", 0.0))
                except (ValueError, TypeError):
                    logger.warning("ignoring malformed head look message: %r", msg.data)
                else:
                    core.look(head_pitch, head_yaw)
    finally:
        core.remove_listener(on_esp32_frame)
        # Removed from the listener set first, so nothing can enqueue
        # after this point, then the pump is cancelled -- otherwise a
        # frame arriving during teardown would sit in a queue nobody
        # drains.
        pump_task.cancel()
        logger.info("control client disconnected (%s)", request.remote)
        await core.client_disconnected()

    return ws
