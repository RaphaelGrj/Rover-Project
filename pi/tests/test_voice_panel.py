"""Tests for rover_control.voice_panel.VoicePanel -- the STT -> IA ->
TTS orchestration behind /audio/converse. AIPanel/AudioPanel are stood
up with monkeypatched provider factories (same pattern as
test_ai_panel.py/test_audio_panel.py) so this never touches a real
network; the point of these tests is the sequencing and per-stage error
labeling, not any one provider's own behavior (already covered
elsewhere).
"""
from __future__ import annotations

import asyncio

import rover_control.ai_panel as ai_panel_module
import rover_control.audio_panel as audio_panel_module
from rover_ai.provider import AIProvider, AIProviderError
from rover_audio.errors import AudioProviderError
from rover_audio.stt import SpeechToTextProvider
from rover_audio.tts import TextToSpeechProvider
from rover_control.ai_panel import AIPanel
from rover_control.audio_panel import AudioPanel
from rover_control.voice_panel import VoicePanel, VoiceTurnError


def run(coro):
    return asyncio.run(coro)


class FakeAIProvider(AIProvider):
    def __init__(self, available: bool = True, reply: str = "réponse de Rover") -> None:
        self.available = available
        self._reply = reply

    async def ask(self, message, context=None):
        if not self.available:
            raise AIProviderError("IA not configured")
        return self._reply

    async def close(self):
        pass


class FakeSTT(SpeechToTextProvider):
    def __init__(self, available: bool = True, text: str = "quelle heure est-il ?") -> None:
        self.available = available
        self._text = text

    async def transcribe(self, audio, *, mime_type: str = "audio/wav") -> str:
        if not self.available:
            raise AudioProviderError("STT not configured")
        return self._text

    async def close(self):
        pass


class FakeTTS(TextToSpeechProvider):
    def __init__(self, available: bool = True, audio: bytes = b"reply-audio-bytes") -> None:
        self.available = available
        self._audio = audio

    async def synthesize(self, text: str) -> bytes:
        if not self.available:
            raise AudioProviderError("TTS not configured")
        return self._audio

    async def close(self):
        pass


def _make_ai_panel(monkeypatch, tmp_path, provider: FakeAIProvider) -> AIPanel:
    monkeypatch.setattr(ai_panel_module, "create_provider", lambda creds: provider)
    return AIPanel(tmp_path / "ai_credentials.json")


def _make_audio_panel(monkeypatch, tmp_path, stt: FakeSTT, tts: FakeTTS) -> AudioPanel:
    monkeypatch.setattr(audio_panel_module, "create_stt_provider", lambda creds: stt)
    monkeypatch.setattr(audio_panel_module, "create_tts_provider", lambda creds: tts)
    return AudioPanel(tmp_path / "audio_credentials.json")


def test_converse_runs_stt_then_ai_then_tts(tmp_path, monkeypatch):
    ai = _make_ai_panel(monkeypatch, tmp_path, FakeAIProvider(reply="il est midi"))
    audio = _make_audio_panel(
        monkeypatch, tmp_path, FakeSTT(text="quelle heure est-il ?"), FakeTTS(audio=b"AUDIO-REPLY")
    )
    voice = VoicePanel(audio, ai)

    heard_text, reply_text, reply_audio = run(voice.converse(b"raw-audio-in"))

    assert heard_text == "quelle heure est-il ?"
    assert reply_text == "il est midi"
    assert reply_audio == b"AUDIO-REPLY"


def test_converse_stt_failure_is_labeled_and_skips_ai_and_tts(tmp_path, monkeypatch):
    ai_provider = FakeAIProvider()
    ai = _make_ai_panel(monkeypatch, tmp_path, ai_provider)
    audio = _make_audio_panel(monkeypatch, tmp_path, FakeSTT(available=False), FakeTTS())
    voice = VoicePanel(audio, ai)

    try:
        run(voice.converse(b"x"))
    except VoiceTurnError as exc:
        assert exc.stage == "stt"
    else:
        raise AssertionError("expected VoiceTurnError")


def test_converse_ai_failure_is_labeled(tmp_path, monkeypatch):
    ai = _make_ai_panel(monkeypatch, tmp_path, FakeAIProvider(available=False))
    audio = _make_audio_panel(monkeypatch, tmp_path, FakeSTT(), FakeTTS())
    voice = VoicePanel(audio, ai)

    try:
        run(voice.converse(b"x"))
    except VoiceTurnError as exc:
        assert exc.stage == "ai"
    else:
        raise AssertionError("expected VoiceTurnError")


def test_converse_tts_failure_is_labeled(tmp_path, monkeypatch):
    ai = _make_ai_panel(monkeypatch, tmp_path, FakeAIProvider())
    audio = _make_audio_panel(monkeypatch, tmp_path, FakeSTT(), FakeTTS(available=False))
    voice = VoicePanel(audio, ai)

    try:
        run(voice.converse(b"x"))
    except VoiceTurnError as exc:
        assert exc.stage == "tts"
    else:
        raise AssertionError("expected VoiceTurnError")


def test_converse_passes_heard_text_as_the_ai_message(tmp_path, monkeypatch):
    # The AI must be asked what was actually heard, not some fixed
    # string -- a regression here would silently ignore the user.
    class RecordingAIProvider(FakeAIProvider):
        def __init__(self):
            super().__init__()
            self.asked_with = None

        async def ask(self, message, context=None):
            self.asked_with = message
            return await super().ask(message, context)

    recording = RecordingAIProvider()
    ai = _make_ai_panel(monkeypatch, tmp_path, recording)
    audio = _make_audio_panel(monkeypatch, tmp_path, FakeSTT(text="allume la lumière"), FakeTTS())
    voice = VoicePanel(audio, ai)

    run(voice.converse(b"x"))

    assert recording.asked_with == "allume la lumière"
