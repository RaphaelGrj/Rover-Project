"""Shared failure type for rover_audio's STT and TTS providers alike --
one error, one degraded-mode contract (ARCHITECTURE_AND_ROADMAP.md §21),
for anything that talks over the network to a speech backend. Kept
separate from stt.py/tts.py so neither interface module has to import
the other just to share this.
"""
from __future__ import annotations


class AudioProviderError(Exception):
    """An STT or TTS provider failed to do its job -- network error, HTTP
    error, malformed response, or simply not configured. Callers must
    catch this and fall back to degraded mode, never let it propagate
    unhandled (same contract as rover_ai.AIProviderError)."""
