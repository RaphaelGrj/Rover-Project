"""Tests for rover_audio -- STT/TTS providers, credential storage, and
the factory that picks one of each from stored config. Never touches a
real network: each provider's HTTP call is intercepted at
_get_session() with a fake aiohttp-shaped session/response, same
pattern as test_rover_ai.py.
"""
from __future__ import annotations

import asyncio
import json
import stat
from typing import Any

from rover_audio import credentials as credentials_module
from rover_audio.cloud import OpenAITTS, OpenAIWhisperSTT
from rover_audio.errors import AudioProviderError
from rover_audio.factory import create_stt_provider, create_tts_provider
from rover_audio.local import LocalSTT, LocalTTS


def run(coro):
    return asyncio.run(coro)


class _FakeResponse:
    def __init__(
        self,
        status: int,
        payload: Any = None,
        text: str = "",
        body: bytes = b"",
        json_error: Exception | None = None,
    ) -> None:
        self.status = status
        self._payload = payload
        self._text = text
        self._body = body
        self._json_error = json_error

    async def json(self):
        if self._json_error is not None:
            raise self._json_error
        return self._payload

    async def text(self):
        return self._text

    async def read(self):
        return self._body

    async def __aenter__(self):
        return self

    async def __aexit__(self, *exc_info):
        return False


class _FakeSession:
    def __init__(self, response: _FakeResponse) -> None:
        self._response = response
        self.calls: list[dict[str, Any]] = []
        self.closed = False

    async def post(self, url, *, headers=None, json=None, data=None):
        self.calls.append({"url": url, "headers": headers, "json": json, "data": data})
        return self._response

    async def close(self):
        self.closed = True


def _wire_fake_session(provider, response: _FakeResponse) -> _FakeSession:
    session = _FakeSession(response)

    async def fake_get_session():
        return session

    provider._get_session = fake_get_session
    return session


def _expect_audio_provider_error(coro):
    try:
        run(coro)
    except AudioProviderError:
        pass
    else:
        raise AssertionError("expected AudioProviderError")


# --- OpenAIWhisperSTT ------------------------------------------------


def test_openai_whisper_stt_transcribes():
    provider = OpenAIWhisperSTT(api_key="sk-test")
    assert provider.available is True
    session = _wire_fake_session(provider, _FakeResponse(200, payload={"text": "bonjour rover"}))

    reply = run(provider.transcribe(b"fake-wav-bytes"))

    assert reply == "bonjour rover"
    call = session.calls[0]
    assert call["url"] == "https://api.openai.com/v1/audio/transcriptions"
    assert call["headers"]["Authorization"] == "Bearer sk-test"


def test_openai_whisper_stt_unconfigured_without_api_key():
    provider = OpenAIWhisperSTT(api_key=None)
    assert provider.available is False
    _expect_audio_provider_error(provider.transcribe(b"x"))


def test_openai_whisper_stt_http_error_becomes_audio_provider_error():
    provider = OpenAIWhisperSTT(api_key="sk-test")
    _wire_fake_session(provider, _FakeResponse(500, text="server error"))
    _expect_audio_provider_error(provider.transcribe(b"x"))


def test_openai_whisper_stt_malformed_response_becomes_audio_provider_error():
    provider = OpenAIWhisperSTT(api_key="sk-test")
    _wire_fake_session(provider, _FakeResponse(200, payload={"unexpected": "shape"}))
    _expect_audio_provider_error(provider.transcribe(b"x"))


# --- OpenAITTS ---------------------------------------------------------


def test_openai_tts_synthesizes():
    provider = OpenAITTS(api_key="sk-test")
    session = _wire_fake_session(provider, _FakeResponse(200, body=b"\xff\xfbMP3DATA"))

    audio = run(provider.synthesize("bonjour"))

    assert audio == b"\xff\xfbMP3DATA"
    call = session.calls[0]
    assert call["url"] == "https://api.openai.com/v1/audio/speech"
    assert call["json"]["input"] == "bonjour"
    assert call["json"]["voice"] == "alloy"


def test_openai_tts_unconfigured_without_api_key():
    provider = OpenAITTS(api_key=None)
    assert provider.available is False
    _expect_audio_provider_error(provider.synthesize("x"))


def test_openai_tts_http_error_becomes_audio_provider_error():
    provider = OpenAITTS(api_key="sk-test")
    _wire_fake_session(provider, _FakeResponse(503, text="upstream down"))
    _expect_audio_provider_error(provider.synthesize("x"))


# --- LocalSTT / LocalTTS -------------------------------------------------


def test_local_stt_transcribes_against_configured_server():
    provider = LocalSTT(base_url="http://192.168.1.50:8000")
    assert provider.available is True
    session = _wire_fake_session(provider, _FakeResponse(200, payload={"text": "salut"}))

    reply = run(provider.transcribe(b"x"))

    assert reply == "salut"
    assert session.calls[0]["url"] == "http://192.168.1.50:8000/audio/transcriptions"


def test_local_stt_unconfigured_without_url():
    provider = LocalSTT(base_url=None)
    assert provider.available is False
    _expect_audio_provider_error(provider.transcribe(b"x"))


def test_local_tts_synthesizes_against_configured_server():
    provider = LocalTTS(base_url="http://192.168.1.51:8001", voice="fr-default")
    session = _wire_fake_session(provider, _FakeResponse(200, body=b"WAVDATA"))

    audio = run(provider.synthesize("salut"))

    assert audio == b"WAVDATA"
    call = session.calls[0]
    assert call["url"] == "http://192.168.1.51:8001/audio/speech"
    assert call["json"]["voice"] == "fr-default"


def test_local_tts_unconfigured_without_url():
    provider = LocalTTS(base_url=None)
    assert provider.available is False
    _expect_audio_provider_error(provider.synthesize("x"))


# --- credentials ---------------------------------------------------------


def test_load_credentials_missing_file_returns_empty(tmp_path):
    assert credentials_module.load_credentials(tmp_path / "nope.json") == {}


def test_save_and_load_round_trip(tmp_path):
    path = tmp_path / "audio_credentials.json"
    data = {
        "stt_provider": "local",
        "stt_local_url": "http://x",
        "tts_provider": "cloud",
        "tts_cloud_vendor": "openai",
        "tts_api_key": "sk-real",
    }

    credentials_module.save_credentials(data, path)

    assert credentials_module.load_credentials(path) == data
    mode = stat.S_IMODE(path.stat().st_mode)
    assert mode == stat.S_IRUSR | stat.S_IWUSR


def test_save_credentials_rejects_unknown_key(tmp_path):
    path = tmp_path / "audio_credentials.json"
    try:
        credentials_module.save_credentials({"totally_made_up": "x"}, path)
    except ValueError:
        pass
    else:
        raise AssertionError("expected ValueError")


def test_load_credentials_ignores_unknown_keys(tmp_path):
    path = tmp_path / "audio_credentials.json"
    path.write_text(json.dumps({"stt_provider": "local", "totally_made_up": "x"}), encoding="utf-8")

    assert credentials_module.load_credentials(path) == {"stt_provider": "local"}


def test_load_credentials_handles_corrupt_json(tmp_path):
    path = tmp_path / "audio_credentials.json"
    path.write_text("{not valid json", encoding="utf-8")

    assert credentials_module.load_credentials(path) == {}


# --- factory ---------------------------------------------------------------


def test_factory_builds_cloud_stt_provider():
    provider = create_stt_provider({"stt_provider": "cloud", "stt_cloud_vendor": "openai", "stt_api_key": "sk-x"})
    assert isinstance(provider, OpenAIWhisperSTT)
    assert provider.available is True


def test_factory_builds_local_stt_provider():
    provider = create_stt_provider({"stt_provider": "local", "stt_local_url": "http://x"})
    assert isinstance(provider, LocalSTT)


def test_factory_unconfigured_stt_when_empty():
    provider = create_stt_provider({})
    assert provider.available is False
    _expect_audio_provider_error(provider.transcribe(b"x"))


def test_factory_unconfigured_stt_when_unknown_vendor():
    provider = create_stt_provider({"stt_provider": "cloud", "stt_cloud_vendor": "not-a-real-vendor"})
    assert provider.available is False


def test_factory_unconfigured_stt_when_vendor_is_not_a_string():
    # Regression guard mirroring rover_ai.factory's own: a malformed
    # config body with a list/dict cloud_vendor must not crash with a
    # TypeError from dict.get() on an unhashable value.
    provider = create_stt_provider({"stt_provider": "cloud", "stt_cloud_vendor": ["not", "a", "string"]})
    assert provider.available is False


def test_factory_builds_cloud_tts_provider():
    provider = create_tts_provider({"tts_provider": "cloud", "tts_cloud_vendor": "openai", "tts_api_key": "sk-x"})
    assert isinstance(provider, OpenAITTS)
    assert provider.available is True


def test_factory_builds_local_tts_provider():
    provider = create_tts_provider({"tts_provider": "local", "tts_local_url": "http://x"})
    assert isinstance(provider, LocalTTS)


def test_factory_unconfigured_tts_when_empty():
    provider = create_tts_provider({})
    assert provider.available is False
    _expect_audio_provider_error(provider.synthesize("x"))


def test_factory_unconfigured_tts_when_vendor_is_not_a_string():
    provider = create_tts_provider({"tts_provider": "cloud", "tts_cloud_vendor": {"not": "a string"}})
    assert provider.available is False
