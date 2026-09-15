"""Web panel logic for rover_audio's STT/TTS providers
(ARCHITECTURE_AND_ROADMAP.md Phase 7) -- same "configure/test from the
existing authenticated web app, no redeploy" idea as ai_panel.py, just
for speech instead of text conversation. Lets the operator test
Speech-to-Text (upload/record a short clip, get text back) and
Text-to-Speech (type text, get audio back) before any microphone/
speaker hardware (Phase 7 "Micro"/"Audio pipeline") exists on the robot
itself -- useful right now from any phone or PC with this page open.

Kept separate from server.py's route handlers, same "business logic
tested, raw aiohttp wiring isn't" split as ai_panel.py/camera.py.
"""
from __future__ import annotations

from pathlib import Path
from typing import Any

from rover_audio import create_stt_provider, create_tts_provider, load_credentials, save_credentials
from rover_audio.credentials import DEFAULT_CREDENTIALS_PATH

# The only fields a client is ever allowed to read or write -- same
# permissive-but-safe stance as ai_panel.CONFIG_FIELDS.
CONFIG_FIELDS = (
    "stt_provider", "stt_cloud_vendor", "stt_api_key", "stt_model", "stt_local_url",
    "tts_provider", "tts_cloud_vendor", "tts_api_key", "tts_model", "tts_voice", "tts_local_url",
)

REDACTED_API_KEY = "********"


def _merge_api_key(new_data: dict[str, Any], existing: dict[str, Any], *, provider_key: str, vendor_key: str, api_key_key: str) -> None:
    """Same "blank field keeps the existing key, but never across a
    vendor switch" convention as ai_panel.AIPanel.update -- see that
    module's docstring/history for the credential-leak bug this guards
    against (switching vendor while leaving the key field blank must
    never carry the OLD vendor's secret into the NEW vendor's slot).
    Mutates new_data in place; called once for STT's fields, once for
    TTS's -- they're independent, each can point at a different vendor.
    """
    api_key = new_data.get(api_key_key)
    blank_or_placeholder = not api_key or api_key == REDACTED_API_KEY

    if new_data.get(provider_key) != "cloud":
        new_data.pop(api_key_key, None)
    elif blank_or_placeholder:
        same_vendor = new_data.get(vendor_key) == existing.get(vendor_key)
        prior = existing.get(api_key_key) if same_vendor else None
        if prior:
            new_data[api_key_key] = prior
        else:
            new_data.pop(api_key_key, None)
    # else: a real new key was typed in -- keep it as given, whether or
    # not the vendor also changed.


class AudioPanel:
    def __init__(self, credentials_path: Path | None = None) -> None:
        self._path = credentials_path or DEFAULT_CREDENTIALS_PATH
        self._credentials = load_credentials(self._path)
        self._stt = create_stt_provider(self._credentials)
        self._tts = create_tts_provider(self._credentials)

    def public_config(self) -> dict[str, Any]:
        data = {k: v for k, v in self._credentials.items() if k in CONFIG_FIELDS}
        if data.get("stt_api_key"):
            data["stt_api_key"] = REDACTED_API_KEY
        if data.get("tts_api_key"):
            data["tts_api_key"] = REDACTED_API_KEY
        data["stt_available"] = self._stt.available
        data["tts_available"] = self._tts.available
        return data

    async def update(self, new_data: dict[str, Any]) -> None:
        """Full replace, not a merge -- same reasoning as
        ai_panel.AIPanel.update: the settings form always submits the
        complete desired state for both STT and TTS."""
        new_data = {k: new_data.get(k) for k in CONFIG_FIELDS if k in new_data}

        _merge_api_key(
            new_data, self._credentials,
            provider_key="stt_provider", vendor_key="stt_cloud_vendor", api_key_key="stt_api_key",
        )
        _merge_api_key(
            new_data, self._credentials,
            provider_key="tts_provider", vendor_key="tts_cloud_vendor", api_key_key="tts_api_key",
        )

        cleaned = {k: v for k, v in new_data.items() if v not in (None, "")}

        # Save FIRST, then swap the providers -- same reasoning as
        # ai_panel.AIPanel.update: closing before a write that can fail
        # would leave this panel holding closed STT/TTS sessions, dead
        # until the process restarts.
        save_credentials(cleaned, self._path)
        await self._stt.close()
        await self._tts.close()
        self._credentials = cleaned
        self._stt = create_stt_provider(cleaned)
        self._tts = create_tts_provider(cleaned)

    async def transcribe(self, audio: bytes, *, mime_type: str = "audio/wav") -> str:
        return await self._stt.transcribe(audio, mime_type=mime_type)

    async def synthesize(self, text: str) -> bytes:
        return await self._tts.synthesize(text)

    async def close(self) -> None:
        await self._stt.close()
        await self._tts.close()
