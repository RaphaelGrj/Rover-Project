"""Picks the concrete SpeechToTextProvider/TextToSpeechProvider from
stored credentials (rover_audio.credentials) -- same decoupling as
rover_ai.factory.create_provider, just two independent choices (STT,
TTS) instead of one, so callers never import a specific vendor class.
"""
from __future__ import annotations

import logging
from typing import Any

from .cloud import OpenAITTS, OpenAIWhisperSTT
from .errors import AudioProviderError
from .local import LocalSTT, LocalTTS
from .stt import SpeechToTextProvider
from .tts import TextToSpeechProvider

logger = logging.getLogger(__name__)

_CLOUD_STT_VENDORS = {"openai": OpenAIWhisperSTT}
_CLOUD_TTS_VENDORS = {"openai": OpenAITTS}


class _UnconfiguredSTT(SpeechToTextProvider):
    available = False

    async def transcribe(self, audio: bytes, *, mime_type: str = "audio/wav") -> str:
        raise AudioProviderError("rover_audio STT is not configured yet")


class _UnconfiguredTTS(TextToSpeechProvider):
    available = False

    async def synthesize(self, text: str) -> bytes:
        raise AudioProviderError("rover_audio TTS is not configured yet")


def create_stt_provider(credentials: dict[str, Any]) -> SpeechToTextProvider:
    kind = credentials.get("stt_provider")

    if kind == "cloud":
        vendor = credentials.get("stt_cloud_vendor")
        # isinstance guard first -- credentials can come straight from a
        # future config panel's JSON body, where stt_cloud_vendor could
        # be a list/dict/etc. instead of a string (same TypeError-on-
        # dict.get() guard as rover_ai.factory.create_provider).
        cls = _CLOUD_STT_VENDORS.get(vendor) if isinstance(vendor, str) else None
        if cls is None:
            logger.warning("unknown or missing stt_cloud_vendor %r -- STT stays unconfigured", vendor)
            return _UnconfiguredSTT()
        return cls(credentials.get("stt_api_key"), credentials.get("stt_model"))

    if kind == "local":
        return LocalSTT(credentials.get("stt_local_url"), credentials.get("stt_model"))

    if kind is not None:
        logger.warning("unknown rover_audio STT provider kind %r -- staying unconfigured", kind)
    return _UnconfiguredSTT()


def create_tts_provider(credentials: dict[str, Any]) -> TextToSpeechProvider:
    kind = credentials.get("tts_provider")

    if kind == "cloud":
        vendor = credentials.get("tts_cloud_vendor")
        cls = _CLOUD_TTS_VENDORS.get(vendor) if isinstance(vendor, str) else None
        if cls is None:
            logger.warning("unknown or missing tts_cloud_vendor %r -- TTS stays unconfigured", vendor)
            return _UnconfiguredTTS()
        return cls(credentials.get("tts_api_key"), credentials.get("tts_model"), credentials.get("tts_voice"))

    if kind == "local":
        return LocalTTS(
            credentials.get("tts_local_url"), credentials.get("tts_model"), credentials.get("tts_voice")
        )

    if kind is not None:
        logger.warning("unknown rover_audio TTS provider kind %r -- staying unconfigured", kind)
    return _UnconfiguredTTS()
