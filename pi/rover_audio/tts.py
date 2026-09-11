"""Text-to-Speech provider interface (ARCHITECTURE_AND_ROADMAP.md Phase 7,
"Text-to-Speech"): turns Rover's reply text (rover_ai's provider output,
already shaped by rover_ai.personality.PersonalityEngine) into audio
bytes, ready to play back through a speaker once Phase 7's audio
pipeline/amp+speaker (BOM: MAX98357A) are wired up.
"""
from __future__ import annotations

from .errors import AudioProviderError

__all__ = ["AudioProviderError", "TextToSpeechProvider"]


class TextToSpeechProvider:
    """Base class for every concrete TTS backend (cloud vendor or local
    server). Same "hardware/feature optional" `available` convention as
    SpeechToTextProvider/rover_ai.AIProvider."""

    available: bool = False

    async def synthesize(self, text: str) -> bytes:
        """Returns encoded audio bytes -- format is provider-specific
        (each concrete provider documents its own, e.g. MP3)."""
        raise NotImplementedError

    async def close(self) -> None:
        """Release any held resources (HTTP session). No-op by default
        -- overridden by providers that actually open one."""
        return None
