"""Shared HTTP plumbing for rover_audio's STT/TTS providers -- same
"session lifecycle managed once, network/HTTP failures turned into one
error type" idea as rover_ai/_http.py, adapted for the two
request/response shapes speech providers use instead of rover_ai's
JSON-in/JSON-out: STT uploads an audio file and gets JSON back, TTS
posts JSON and gets raw audio bytes back.

A pure mixin (no interface methods of its own) -- concrete providers
combine this with SpeechToTextProvider or TextToSpeechProvider (see
cloud.py/local.py), same split as rover_ai's `_HttpAIProvider(AIProvider)`
would be if rover_ai had more than one interface to share HTTP plumbing
across.
"""
from __future__ import annotations

import asyncio
import json
import logging

import aiohttp

from .errors import AudioProviderError

logger = logging.getLogger(__name__)

# Same rationale as rover_ai/_http.py: generous relative to a normal
# call so a slow-but-working provider (a cloud STT/TTS call, or a local
# server on modest hardware) isn't misreported as unavailable. TTS/STT
# payloads are audio, typically slower to transfer than a chat reply.
_CONNECT_TIMEOUT_S = 10
_READ_TIMEOUT_S = 60


class _HttpAudioClient:
    def __init__(self) -> None:
        self._session: aiohttp.ClientSession | None = None

    async def _get_session(self) -> aiohttp.ClientSession:
        # Created lazily, inside the running event loop that will
        # actually use it -- same pattern as rover_ai/_http.py and
        # rover_control.camera.CameraStream._get_session.
        if self._session is None or self._session.closed:
            self._session = aiohttp.ClientSession(
                timeout=aiohttp.ClientTimeout(connect=_CONNECT_TIMEOUT_S, sock_read=_READ_TIMEOUT_S)
            )
        return self._session

    async def close(self) -> None:
        if self._session is not None and not self._session.closed:
            await self._session.close()

    async def _post_multipart_for_json(
        self, url: str, *, headers: dict[str, str], data: aiohttp.FormData
    ) -> dict:
        """STT shape: upload an audio file, get a JSON body back (eg.
        OpenAI's /audio/transcriptions {"text": "..."})."""
        session = await self._get_session()
        try:
            response = await session.post(url, headers=headers, data=data)
        except (aiohttp.ClientError, asyncio.TimeoutError) as exc:
            raise AudioProviderError(f"network error contacting {url}: {exc}") from exc

        async with response:
            try:
                if response.status != 200:
                    body = await response.text()
                    raise AudioProviderError(f"{url} returned HTTP {response.status}: {body[:200]!r}")
                return await response.json()
            except (aiohttp.ClientError, asyncio.TimeoutError) as exc:
                raise AudioProviderError(f"network error reading response from {url}: {exc}") from exc
            except json.JSONDecodeError as exc:
                raise AudioProviderError(f"{url} returned malformed JSON: {exc}") from exc

    async def _post_json_for_bytes(self, url: str, *, headers: dict[str, str], json_body: dict) -> bytes:
        """TTS shape: post text (+ options) as JSON, get raw audio bytes
        back (eg. OpenAI's /audio/speech, an MP3 body)."""
        session = await self._get_session()
        try:
            response = await session.post(url, headers=headers, json=json_body)
        except (aiohttp.ClientError, asyncio.TimeoutError) as exc:
            raise AudioProviderError(f"network error contacting {url}: {exc}") from exc

        async with response:
            try:
                if response.status != 200:
                    body = await response.text()
                    raise AudioProviderError(f"{url} returned HTTP {response.status}: {body[:200]!r}")
                return await response.read()
            except (aiohttp.ClientError, asyncio.TimeoutError) as exc:
                raise AudioProviderError(f"network error reading response from {url}: {exc}") from exc
