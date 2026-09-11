"""Personality Engine (ARCHITECTURE_AND_ROADMAP.md §15, Phase 7 checklist
"Personality Engine" / "Machine à émotions") -- the piece `provider.py`'s
`AIContext` docstring already anticipated ("system is a system-style
instruction... once the personality engine exists"): builds Rover's
persona + conversation history into the `AIContext` every provider
already knows how to consume (see cloud.py/_openai_compatible.py), and
reacts to a conversation turn with an emotion.

Kept in `rover_ai` rather than `rover_core`: it exists specifically to
power a rover-ai conversation, and `rover_control` (where it's actually
used, see `ai_panel.py`) never imports `rover_core` at runtime (only
under `TYPE_CHECKING` in server.py) -- putting this here instead avoids
adding a new runtime edge against that boundary. The emotion reaction
reaches RoverCore through a plain callback (`emotion_sink`), the same
"caller passes its own hook" pattern RoverCore already uses for its own
`FrameListener` -- this module never needs to import RoverCore itself.
"""
from __future__ import annotations

import logging
from typing import Awaitable, Callable

from .provider import AIContext, AIProviderError

logger = logging.getLogger(__name__)

# Matches esp32/lib/emotion/Emotion.h and ARCHITECTURE_AND_ROADMAP.md §15
# exactly -- the only names a FACE emotion=... frame is allowed to carry.
EMOTIONS = ("idle", "happy", "curious", "sleepy", "confused", "alert", "sad", "excited")

SYSTEM_PROMPT = (
    "Tu es Rover, un compagnon robotique domestique développé par R-Bot, "
    "successeur mobile de LUMI. Tu es curieux, chaleureux et un peu joueur, "
    "avec une esthétique \"Glitch\" assumée. Réponds en français, de façon "
    "concise et naturelle (une à trois phrases) -- pas de listes ni de "
    "formatage markdown, ce texte est destiné à être lu à voix haute une "
    "fois le Text-to-Speech de la Phase 7 branché."
)

# Bounded so the context sent to a cloud provider (cost, latency) never
# grows without limit over a long conversation -- oldest turns drop first.
MAX_HISTORY_TURNS = 10

EmotionSink = Callable[[str], None]
AskFn = Callable[[str, AIContext], Awaitable[str]]


class PersonalityEngine:
    """Owns the conversation history + persona (§15) for one rover-ai
    "brain", independent of which AIProvider or transport (the text panel
    today, voice once Phase 7's STT/TTS exists) is driving it."""

    def __init__(self, emotion_sink: EmotionSink | None = None) -> None:
        self._emotion_sink = emotion_sink
        self._history: list[tuple[str, str]] = []

    def build_context(self) -> AIContext:
        return AIContext(system=SYSTEM_PROMPT, history=list(self._history))

    def reset(self) -> None:
        """Starts a fresh conversation -- no memory of prior turns.
        Called on an explicit user action (the panel's "new conversation")
        and by AIPanel.update() (ARCHITECTURE_AND_ROADMAP.md §17.1: a
        reconfigured provider/vendor should not silently inherit history
        built against a different backend)."""
        self._history.clear()

    async def converse(self, ask: AskFn, message: str) -> str:
        """Wraps one `AIProvider.ask` call with the emotion reaction and
        history bookkeeping every caller (text panel, later voice) would
        otherwise have to duplicate."""
        self._emote("curious")
        try:
            reply = await ask(message, self.build_context())
        except AIProviderError:
            # §21 "mode dégradé": Rover just looks confused about it and
            # re-raises -- the caller (ai_panel.ask) still decides how to
            # surface the failure (currently a 503, see server.py).
            self._emote("confused")
            raise
        self._remember(message, reply)
        self._emote("happy")
        return reply

    def _remember(self, message: str, reply: str) -> None:
        self._history.append(("user", message))
        self._history.append(("assistant", reply))
        overflow = len(self._history) - MAX_HISTORY_TURNS * 2
        if overflow > 0:
            del self._history[:overflow]

    def _emote(self, emotion: str) -> None:
        if self._emotion_sink is None:
            return
        if emotion not in EMOTIONS:
            logger.warning("ignoring unknown emotion %r", emotion)
            return
        try:
            self._emotion_sink(emotion)
        except Exception:
            # A cosmetic reaction must never take down a conversation --
            # log and keep going regardless of what went wrong getting
            # this to the ESP32 (serial link down, RoverCore mid-teardown).
            logger.warning("emotion_sink failed for emotion=%s", emotion, exc_info=True)
