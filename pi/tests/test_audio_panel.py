"""Tests for rover_control.audio_panel.AudioPanel -- the logic behind
the /audio/* routes (rover_control/server.py). create_stt_provider/
create_tts_provider are monkeypatched here so these tests never build a
real network-backed provider, same pattern as test_ai_panel.py.
"""
from __future__ import annotations

import asyncio

import rover_control.audio_panel as audio_panel_module
from rover_audio.errors import AudioProviderError
from rover_audio.stt import SpeechToTextProvider
from rover_audio.tts import TextToSpeechProvider
from rover_control.audio_panel import AudioPanel, REDACTED_API_KEY


def run(coro):
    return asyncio.run(coro)


class FakeSTT(SpeechToTextProvider):
    def __init__(self, available: bool = True, text: str = "fake transcript") -> None:
        self.available = available
        self._text = text
        self.closed = False

    async def transcribe(self, audio, *, mime_type: str = "audio/wav") -> str:
        if not self.available:
            raise AudioProviderError("not configured")
        return self._text

    async def close(self):
        self.closed = True


class FakeTTS(TextToSpeechProvider):
    def __init__(self, available: bool = True, audio: bytes = b"fake-audio") -> None:
        self.available = available
        self._audio = audio
        self.closed = False
        self.asked_with: str | None = None

    async def synthesize(self, text: str) -> bytes:
        self.asked_with = text
        if not self.available:
            raise AudioProviderError("not configured")
        return self._audio

    async def close(self):
        self.closed = True


def _stub_factories(monkeypatch, stt_factory=None, tts_factory=None):
    built: dict[str, list] = {"stt": [], "tts": []}

    def fake_create_stt(credentials):
        built["stt"].append(dict(credentials))
        return stt_factory(credentials) if stt_factory else FakeSTT()

    def fake_create_tts(credentials):
        built["tts"].append(dict(credentials))
        return tts_factory(credentials) if tts_factory else FakeTTS()

    monkeypatch.setattr(audio_panel_module, "create_stt_provider", fake_create_stt)
    monkeypatch.setattr(audio_panel_module, "create_tts_provider", fake_create_tts)
    return built


def test_public_config_redacts_both_api_keys(tmp_path, monkeypatch):
    _stub_factories(monkeypatch)
    path = tmp_path / "audio_credentials.json"
    path.write_text(
        '{"stt_provider": "cloud", "stt_cloud_vendor": "openai", "stt_api_key": "sk-stt-secret", '
        '"tts_provider": "cloud", "tts_cloud_vendor": "openai", "tts_api_key": "sk-tts-secret"}',
        encoding="utf-8",
    )

    panel = AudioPanel(path)
    config = panel.public_config()

    assert config["stt_api_key"] == REDACTED_API_KEY
    assert config["tts_api_key"] == REDACTED_API_KEY
    assert config["stt_available"] is True
    assert config["tts_available"] is True


def test_public_config_when_unconfigured(tmp_path, monkeypatch):
    _stub_factories(
        monkeypatch,
        stt_factory=lambda c: FakeSTT(available=False),
        tts_factory=lambda c: FakeTTS(available=False),
    )
    panel = AudioPanel(tmp_path / "does-not-exist.json")

    config = panel.public_config()

    assert "stt_api_key" not in config
    assert "tts_api_key" not in config
    assert config["stt_available"] is False
    assert config["tts_available"] is False


def test_update_saves_and_rebuilds_both_providers(tmp_path, monkeypatch):
    built = _stub_factories(monkeypatch)
    path = tmp_path / "audio_credentials.json"

    async def body():
        panel = AudioPanel(path)
        await panel.update({
            "stt_provider": "local", "stt_local_url": "http://192.168.1.50:8000",
            "tts_provider": "local", "tts_local_url": "http://192.168.1.51:8001",
        })

    run(body())

    expected = {
        "stt_provider": "local", "stt_local_url": "http://192.168.1.50:8000",
        "tts_provider": "local", "tts_local_url": "http://192.168.1.51:8001",
    }
    assert built["stt"][-1] == expected
    assert built["tts"][-1] == expected
    assert audio_panel_module.load_credentials(path) == expected


def test_update_ignores_unknown_fields(tmp_path, monkeypatch):
    _stub_factories(monkeypatch)
    path = tmp_path / "audio_credentials.json"

    async def body():
        panel = AudioPanel(path)
        await panel.update({"stt_provider": "local", "stt_local_url": "http://x", "totally_made_up": "x"})

    run(body())

    assert "totally_made_up" not in audio_panel_module.load_credentials(path)


def test_update_keeps_existing_stt_key_when_field_left_blank(tmp_path, monkeypatch):
    _stub_factories(monkeypatch)
    path = tmp_path / "audio_credentials.json"
    path.write_text(
        '{"stt_provider": "cloud", "stt_cloud_vendor": "openai", "stt_api_key": "sk-real-secret"}',
        encoding="utf-8",
    )

    async def body():
        panel = AudioPanel(path)
        await panel.update({"stt_provider": "cloud", "stt_cloud_vendor": "openai", "stt_api_key": ""})

    run(body())

    assert audio_panel_module.load_credentials(path)["stt_api_key"] == "sk-real-secret"


def test_update_does_not_leak_stt_key_into_tts_or_across_vendor_switch(tmp_path, monkeypatch):
    # Regression guard mirroring ai_panel's own real bug (PROGRESS.md,
    # 2026-09-09): blank key field + a vendor change must never silently
    # reuse a DIFFERENT config's secret -- here specifically, STT's key
    # must never leak into a blank TTS key field (they're independent),
    # and switching either one's vendor while blank must drop, not keep.
    _stub_factories(monkeypatch)
    path = tmp_path / "audio_credentials.json"
    path.write_text(
        '{"stt_provider": "cloud", "stt_cloud_vendor": "openai", "stt_api_key": "sk-stt-secret"}',
        encoding="utf-8",
    )

    async def body():
        panel = AudioPanel(path)
        await panel.update({
            "stt_provider": "cloud", "stt_cloud_vendor": "openai", "stt_api_key": "",
            "tts_provider": "cloud", "tts_cloud_vendor": "openai", "tts_api_key": "",
        })

    run(body())

    saved = audio_panel_module.load_credentials(path)
    assert saved["stt_api_key"] == "sk-stt-secret"  # unchanged, same vendor
    assert "tts_api_key" not in saved  # never inherited STT's key


def test_update_keeps_a_freshly_typed_key_even_when_switching_cloud_vendor(tmp_path, monkeypatch):
    _stub_factories(monkeypatch)
    path = tmp_path / "audio_credentials.json"
    path.write_text(
        '{"tts_provider": "cloud", "tts_cloud_vendor": "openai", "tts_api_key": "sk-old-secret"}',
        encoding="utf-8",
    )

    async def body():
        panel = AudioPanel(path)
        await panel.update({"tts_provider": "cloud", "tts_cloud_vendor": "openai", "tts_api_key": "sk-new-secret"})

    run(body())

    assert audio_panel_module.load_credentials(path)["tts_api_key"] == "sk-new-secret"


def test_update_drops_api_key_when_provider_is_not_cloud(tmp_path, monkeypatch):
    _stub_factories(monkeypatch)
    path = tmp_path / "audio_credentials.json"
    path.write_text(
        '{"stt_provider": "cloud", "stt_cloud_vendor": "openai", "stt_api_key": "sk-real-secret"}',
        encoding="utf-8",
    )

    async def body():
        panel = AudioPanel(path)
        await panel.update({"stt_provider": "local", "stt_local_url": "http://x"})

    run(body())

    assert "stt_api_key" not in audio_panel_module.load_credentials(path)


def test_transcribe_delegates_to_stt_provider(tmp_path, monkeypatch):
    fake_stt = FakeSTT(text="bonjour")
    _stub_factories(monkeypatch, stt_factory=lambda c: fake_stt)
    path = tmp_path / "audio_credentials.json"

    async def body():
        panel = AudioPanel(path)
        text = await panel.transcribe(b"raw-audio-bytes")
        assert text == "bonjour"

    run(body())


def test_transcribe_propagates_provider_error(tmp_path, monkeypatch):
    _stub_factories(monkeypatch, stt_factory=lambda c: FakeSTT(available=False))
    path = tmp_path / "audio_credentials.json"

    async def body():
        panel = AudioPanel(path)
        try:
            await panel.transcribe(b"x")
        except AudioProviderError:
            pass
        else:
            raise AssertionError("expected AudioProviderError")

    run(body())


def test_synthesize_delegates_to_tts_provider(tmp_path, monkeypatch):
    fake_tts = FakeTTS(audio=b"synthesized-bytes")
    _stub_factories(monkeypatch, tts_factory=lambda c: fake_tts)
    path = tmp_path / "audio_credentials.json"

    async def body():
        panel = AudioPanel(path)
        audio = await panel.synthesize("bonjour")
        assert audio == b"synthesized-bytes"
        assert fake_tts.asked_with == "bonjour"

    run(body())


def test_close_closes_both_providers(tmp_path, monkeypatch):
    fake_stt, fake_tts = FakeSTT(), FakeTTS()
    _stub_factories(monkeypatch, stt_factory=lambda c: fake_stt, tts_factory=lambda c: fake_tts)
    path = tmp_path / "audio_credentials.json"

    async def body():
        panel = AudioPanel(path)
        await panel.close()
        assert fake_stt.closed is True
        assert fake_tts.closed is True

    run(body())


def test_update_closes_previous_providers_before_building_new_ones(tmp_path, monkeypatch):
    first_stt, first_tts = FakeSTT(), FakeTTS()
    stt_queue = [first_stt, FakeSTT()]
    tts_queue = [first_tts, FakeTTS()]
    _stub_factories(monkeypatch, stt_factory=lambda c: stt_queue.pop(0), tts_factory=lambda c: tts_queue.pop(0))
    path = tmp_path / "audio_credentials.json"

    async def body():
        panel = AudioPanel(path)
        await panel.update({"stt_provider": "local", "stt_local_url": "http://x"})
        assert first_stt.closed is True
        assert first_tts.closed is True

    run(body())
