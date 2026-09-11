"""Cloud STT/TTS providers -- ARCHITECTURE_AND_ROADMAP.md Phase 7. Only
OpenAI for now (Whisper for transcription, its own TTS API for
synthesis) -- rover-ai (§17.1) grew from 3 cloud vendors to 8 once that
shape proved out and the user asked for more; the same growth path is
open here (factory.py's _CLOUD_STT_VENDORS/_CLOUD_TTS_VENDORS), starting
minimal rather than guessing upfront which other vendors are worth it.
"""
from __future__ import annotations

import aiohttp

from ._http import _HttpAudioClient
from .errors import AudioProviderError
from .stt import SpeechToTextProvider
from .tts import TextToSpeechProvider

_OPENAI_BASE_URL = "https://api.openai.com/v1"


class OpenAIWhisperSTT(_HttpAudioClient, SpeechToTextProvider):
    """OpenAI's /audio/transcriptions endpoint (Whisper)."""

    DEFAULT_MODEL = "whisper-1"

    def __init__(self, api_key: str | None, model: str | None = None) -> None:
        super().__init__()
        self._api_key = api_key
        self._model = model or self.DEFAULT_MODEL
        self.available = bool(api_key)

    async def transcribe(self, audio: bytes, *, mime_type: str = "audio/wav") -> str:
        if not self.available:
            raise AudioProviderError("OpenAIWhisperSTT is not configured (missing API key)")

        form = aiohttp.FormData()
        form.add_field("file", audio, filename="audio", content_type=mime_type)
        form.add_field("model", self._model)

        data = await self._post_multipart_for_json(
            f"{_OPENAI_BASE_URL}/audio/transcriptions",
            headers={"Authorization": f"Bearer {self._api_key}"},
            data=form,
        )
        try:
            return data["text"]
        except (KeyError, TypeError) as exc:
            raise AudioProviderError(f"unexpected response shape from OpenAI Whisper: {data!r}") from exc


class OpenAITTS(_HttpAudioClient, TextToSpeechProvider):
    """OpenAI's /audio/speech endpoint. Returns MP3 bytes."""

    DEFAULT_MODEL = "tts-1"
    DEFAULT_VOICE = "alloy"

    def __init__(self, api_key: str | None, model: str | None = None, voice: str | None = None) -> None:
        super().__init__()
        self._api_key = api_key
        self._model = model or self.DEFAULT_MODEL
        self._voice = voice or self.DEFAULT_VOICE
        self.available = bool(api_key)

    async def synthesize(self, text: str) -> bytes:
        if not self.available:
            raise AudioProviderError("OpenAITTS is not configured (missing API key)")

        return await self._post_json_for_bytes(
            f"{_OPENAI_BASE_URL}/audio/speech",
            headers={"Authorization": f"Bearer {self._api_key}", "content-type": "application/json"},
            json_body={"model": self._model, "voice": self._voice, "input": text},
        )
