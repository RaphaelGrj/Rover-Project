"""Speech-to-Text provider interface (ARCHITECTURE_AND_ROADMAP.md Phase 7,
"Speech-to-Text"): turns recorded audio into text. Feeds directly into
rover_ai.personality.PersonalityEngine.converse() -- the same entry
point the /ai text panel already uses (rover_control/ai_panel.py) --
once a microphone (Phase 7 "Micro", not implemented yet) produces audio
to pass in here.

Deliberately its own small interface rather than reusing
rover_ai.provider.AIProvider: different input/output shape (audio bytes
in, plain text out, no conversation context) -- forcing a shared base
class would cost more than it saves.
"""
from __future__ import annotations

from .errors import AudioProviderError

__all__ = ["AudioProviderError", "SpeechToTextProvider"]


class SpeechToTextProvider:
    """Base class for every concrete STT backend (cloud vendor or local
    server). `available` mirrors the same "hardware/feature optional"
    pattern as rover_ai.AIProvider -- False when unconfigured, so a
    caller can check before making a call that was never going to work.
    """

    available: bool = False

    async def transcribe(self, audio: bytes, *, mime_type: str = "audio/wav") -> str:
        raise NotImplementedError

    async def close(self) -> None:
        """Release any held resources (HTTP session). No-op by default
        -- overridden by providers that actually open one."""
        return None
