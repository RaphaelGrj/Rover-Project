"""Orchestrates one full voice turn (ARCHITECTURE_AND_ROADMAP.md §17,
"Audio pipeline"): recorded audio -> Speech-to-Text -> rover-ai
(persona + history, already wired through AIPanel/PersonalityEngine,
§17.1) -> Text-to-Speech -> audio reply. The missing link between
rover_audio (§17.2, STT/TTS) and the conversation loop -- both ends
already exist and are independently tested; this module is only the
sequencing between them, with each step's failure identified so a
caller can tell whether it was transcription, the AI, or synthesis that
failed, instead of one undifferentiated error.

Still not the real "Audio pipeline" checklist item end to end: no
microphone/speaker feed this yet (Phase 7 "Micro" not wired) -- this is
the orchestration a real mic/speaker, or the /audio web panel meanwhile,
calls into.
"""
from __future__ import annotations

from rover_ai import AIProviderError
from rover_audio import AudioProviderError

from .ai_panel import AIPanel
from .audio_panel import AudioPanel


class VoiceTurnError(Exception):
    """One step of the pipeline failed. `stage` is "stt"/"ai"/"tts" --
    lets a caller (the /audio/converse route) report exactly where,
    rather than a single undifferentiated failure (§21 "mode dégradé":
    still just a recoverable, expected condition, never a crash)."""

    def __init__(self, stage: str, message: str) -> None:
        super().__init__(f"{stage}: {message}")
        self.stage = stage


class VoicePanel:
    def __init__(self, audio_panel: AudioPanel, ai_panel: AIPanel) -> None:
        self._audio = audio_panel
        self._ai = ai_panel

    async def converse(self, audio: bytes, *, mime_type: str = "audio/wav") -> tuple[str, str, bytes]:
        """Returns (heard_text, reply_text, reply_audio)."""
        try:
            heard_text = await self._audio.transcribe(audio, mime_type=mime_type)
        except AudioProviderError as exc:
            raise VoiceTurnError("stt", str(exc)) from exc

        try:
            reply_text = await self._ai.ask(heard_text)
        except AIProviderError as exc:
            raise VoiceTurnError("ai", str(exc)) from exc

        try:
            reply_audio = await self._audio.synthesize(reply_text)
        except AudioProviderError as exc:
            raise VoiceTurnError("tts", str(exc)) from exc

        return heard_text, reply_text, reply_audio
