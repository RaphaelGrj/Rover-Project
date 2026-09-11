"""rover_audio -- interchangeable Speech-to-Text and Text-to-Speech
providers (cloud API or local network server), see
ARCHITECTURE_AND_ROADMAP.md Phase 7 ("Speech-to-Text"/"Text-to-Speech").
Public surface: build the active STT/TTS provider from stored
credentials, then call .transcribe()/.synthesize() -- callers never need
to import a specific vendor class.
"""
from __future__ import annotations

from .credentials import load_credentials, save_credentials
from .errors import AudioProviderError
from .factory import create_stt_provider, create_tts_provider
from .stt import SpeechToTextProvider
from .tts import TextToSpeechProvider

__all__ = [
    "AudioProviderError",
    "SpeechToTextProvider",
    "TextToSpeechProvider",
    "create_stt_provider",
    "create_tts_provider",
    "load_credentials",
    "save_credentials",
]
