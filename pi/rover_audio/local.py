"""Local network STT/TTS -- Phase 7's local counterpart to cloud.py,
same idea as rover_ai.local.LocalAIProvider (§17.1): any self-hosted
server that speaks an OpenAI-compatible audio endpoint (eg. a
whisper.cpp/faster-whisper server wrapped to match
`/audio/transcriptions`, or a project like openedai-speech for
`/audio/speech`) -- Rover never needs to know or care what's actually
running on the other end, only which network address and model name to
hit. No API key required, same "trusted local network" stance as
LocalAIProvider.
"""
from __future__ import annotations

import aiohttp

from ._http import _HttpAudioClient
from .errors import AudioProviderError
from .stt import SpeechToTextProvider
from .tts import TextToSpeechProvider


class LocalSTT(_HttpAudioClient, SpeechToTextProvider):
    DEFAULT_MODEL = "whisper-1"

    def __init__(self, base_url: str | None, model: str | None = None) -> None:
        super().__init__()
        self._base_url = (base_url or "").rstrip("/")
        self._model = model or self.DEFAULT_MODEL
        self.available = bool(base_url)

    async def transcribe(self, audio: bytes, *, mime_type: str = "audio/wav") -> str:
        if not self.available:
            raise AudioProviderError("LocalSTT is not configured (missing server address)")

        form = aiohttp.FormData()
        form.add_field("file", audio, filename="audio", content_type=mime_type)
        form.add_field("model", self._model)

        data = await self._post_multipart_for_json(
            f"{self._base_url}/audio/transcriptions", headers={}, data=form
        )
        try:
            return data["text"]
        except (KeyError, TypeError) as exc:
            raise AudioProviderError(f"unexpected response shape from {self._base_url}: {data!r}") from exc


class LocalTTS(_HttpAudioClient, TextToSpeechProvider):
    DEFAULT_MODEL = "tts-1"
    DEFAULT_VOICE = "default"

    def __init__(self, base_url: str | None, model: str | None = None, voice: str | None = None) -> None:
        super().__init__()
        self._base_url = (base_url or "").rstrip("/")
        self._model = model or self.DEFAULT_MODEL
        self._voice = voice or self.DEFAULT_VOICE
        self.available = bool(base_url)

    async def synthesize(self, text: str) -> bytes:
        if not self.available:
            raise AudioProviderError("LocalTTS is not configured (missing server address)")

        return await self._post_json_for_bytes(
            f"{self._base_url}/audio/speech",
            headers={"content-type": "application/json"},
            json_body={"model": self._model, "voice": self._voice, "input": text},
        )
