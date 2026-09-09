"""Tests for rover_ai -- providers, credential storage, and the
factory that picks one from the other. Never touches a real network:
each provider's HTTP call is intercepted at _get_session() with a fake
aiohttp-shaped session/response, same "hardware/network optional"
spirit as test_mqtt_publisher.py, just applied to an HTTP dependency
instead of a broker connection.
"""
from __future__ import annotations

import asyncio
import json
import stat
from typing import Any

import aiohttp

from rover_ai import credentials as credentials_module
from rover_ai.cloud import (
    AnthropicProvider,
    DeepInfraProvider,
    FireworksProvider,
    GeminiProvider,
    OpenAIProvider,
    OpenRouterProvider,
    QwenCloudProvider,
    TogetherProvider,
)
from rover_ai.factory import create_provider
from rover_ai.local import LocalAIProvider
from rover_ai.provider import AIContext, AIProvider, AIProviderError


def run(coro):
    return asyncio.run(coro)


class _FakeResponse:
    def __init__(self, status: int, payload: Any = None, text: str = "", json_error: Exception | None = None) -> None:
        self.status = status
        self._payload = payload
        self._text = text
        self._json_error = json_error

    async def json(self):
        if self._json_error is not None:
            raise self._json_error
        return self._payload

    async def text(self):
        return self._text

    async def __aenter__(self):
        return self

    async def __aexit__(self, *exc_info):
        return False


class _FakeSession:
    def __init__(self, response: _FakeResponse) -> None:
        self._response = response
        self.calls: list[dict[str, Any]] = []
        self.closed = False

    async def post(self, url, *, headers=None, json=None):
        # Real aiohttp's session.post() returns an awaitable (also
        # usable as an async context manager) -- _http.py does
        # `response = await session.post(...)` then `async with
        # response:`, so this fake must be awaitable too, not just
        # return the response synchronously.
        self.calls.append({"url": url, "headers": headers, "json": json})
        return self._response

    async def close(self):
        self.closed = True


def _wire_fake_session(provider, response: _FakeResponse) -> _FakeSession:
    session = _FakeSession(response)

    async def fake_get_session():
        return session

    provider._get_session = fake_get_session
    return session


# --- AIContext / base provider behavior -----------------------------


def test_unconfigured_provider_ask_raises_without_network():
    async def body():
        provider = AnthropicProvider(api_key=None)
        assert provider.available is False
        try:
            await provider.ask("hi")
        except AIProviderError:
            pass
        else:
            raise AssertionError("expected AIProviderError")

    run(body())


def test_close_is_a_no_op_when_never_used():
    async def body():
        provider = AIProvider()
        await provider.close()  # must not raise

    run(body())


# --- AnthropicProvider ------------------------------------------------


def test_anthropic_ask_builds_request_and_parses_reply():
    async def body():
        response = _FakeResponse(200, {"content": [{"text": "hello from claude"}]})
        provider = AnthropicProvider(api_key="sk-ant-test", model="claude-x")
        session = _wire_fake_session(provider, response)

        reply = await provider.ask("hi", AIContext(system="be nice", history=[("user", "prev")]))

        assert reply == "hello from claude"
        call = session.calls[0]
        assert call["url"] == "https://api.anthropic.com/v1/messages"
        assert call["headers"]["x-api-key"] == "sk-ant-test"
        assert call["headers"]["anthropic-version"] == "2023-06-01"
        assert call["json"]["model"] == "claude-x"
        assert call["json"]["system"] == "be nice"
        assert call["json"]["messages"] == [
            {"role": "user", "content": "prev"},
            {"role": "user", "content": "hi"},
        ]

    run(body())


def test_anthropic_ask_raises_on_http_error():
    async def body():
        response = _FakeResponse(401, text="unauthorized")
        provider = AnthropicProvider(api_key="bad-key")
        _wire_fake_session(provider, response)

        try:
            await provider.ask("hi")
        except AIProviderError as exc:
            assert "401" in str(exc)
        else:
            raise AssertionError("expected AIProviderError")

    run(body())


def test_anthropic_ask_raises_on_malformed_response():
    async def body():
        response = _FakeResponse(200, {"unexpected": "shape"})
        provider = AnthropicProvider(api_key="sk-ant-test")
        _wire_fake_session(provider, response)

        try:
            await provider.ask("hi")
        except AIProviderError:
            pass
        else:
            raise AssertionError("expected AIProviderError")

    run(body())


# --- OpenAIProvider (and, by extension, the shared OpenAI-compatible base) --


def test_openai_ask_builds_request_and_parses_reply():
    async def body():
        response = _FakeResponse(200, {"choices": [{"message": {"content": "hello from gpt"}}]})
        provider = OpenAIProvider(api_key="sk-openai-test", model="gpt-x")
        session = _wire_fake_session(provider, response)

        reply = await provider.ask("hi", AIContext(system="be nice"))

        assert reply == "hello from gpt"
        call = session.calls[0]
        assert call["url"] == "https://api.openai.com/v1/chat/completions"
        assert call["headers"]["Authorization"] == "Bearer sk-openai-test"
        assert call["json"]["messages"][0] == {"role": "system", "content": "be nice"}
        assert call["json"]["messages"][-1] == {"role": "user", "content": "hi"}

    run(body())


def test_openai_ask_raises_ai_provider_error_when_body_read_fails():
    # Status/headers came back fine (200), but the body read itself
    # fails mid-transfer -- must still surface as AIProviderError, not
    # an unwrapped aiohttp exception (regression: this used to escape
    # ask() unwrapped, see rover_ai/_http.py).
    async def body():
        response = _FakeResponse(200, json_error=aiohttp.ClientPayloadError("connection dropped mid-body"))
        provider = OpenAIProvider(api_key="sk-openai-test")
        _wire_fake_session(provider, response)

        try:
            await provider.ask("hi")
        except AIProviderError:
            pass
        else:
            raise AssertionError("expected AIProviderError")

    run(body())


def test_openai_ask_raises_ai_provider_error_on_malformed_json_body():
    async def body():
        response = _FakeResponse(200, json_error=json.JSONDecodeError("bad json", "doc", 0))
        provider = OpenAIProvider(api_key="sk-openai-test")
        _wire_fake_session(provider, response)

        try:
            await provider.ask("hi")
        except AIProviderError:
            pass
        else:
            raise AssertionError("expected AIProviderError")

    run(body())


# --- GeminiProvider -----------------------------------------------------


def test_gemini_ask_builds_request_and_parses_reply():
    async def body():
        response = _FakeResponse(
            200, {"candidates": [{"content": {"parts": [{"text": "hello from gemini"}]}}]}
        )
        provider = GeminiProvider(api_key="gk-test", model="gemini-x")
        session = _wire_fake_session(provider, response)

        reply = await provider.ask("hi", AIContext(history=[("assistant", "prev reply")]))

        assert reply == "hello from gemini"
        call = session.calls[0]
        assert call["url"] == "https://generativelanguage.googleapis.com/v1beta/models/gemini-x:generateContent"
        assert call["headers"]["x-goog-api-key"] == "gk-test"
        assert call["json"]["contents"][0] == {"role": "model", "parts": [{"text": "prev reply"}]}
        assert call["json"]["contents"][-1] == {"role": "user", "parts": [{"text": "hi"}]}

    run(body())


# --- OpenAI-compatible aggregators (Qwen cloud, OpenRouter, Together, Fireworks, DeepInfra) --


def test_new_cloud_vendors_use_openai_compatible_shape_with_correct_base_url():
    async def body():
        cases = [
            (QwenCloudProvider, "https://dashscope-intl.aliyuncs.com/compatible-mode/v1", "qwen-plus"),
            (OpenRouterProvider, "https://openrouter.ai/api/v1", "meta-llama/llama-3.1-8b-instruct"),
            (TogetherProvider, "https://api.together.xyz/v1", "Qwen/Qwen2.5-72B-Instruct-Turbo"),
            (FireworksProvider, "https://api.fireworks.ai/inference/v1", "accounts/fireworks/models/qwen2p5-72b-instruct"),
            (DeepInfraProvider, "https://api.deepinfra.com/v1/openai", "Qwen/Qwen2.5-72B-Instruct"),
        ]
        for provider_cls, base_url, default_model in cases:
            response = _FakeResponse(200, {"choices": [{"message": {"content": "ok"}}]})
            provider = provider_cls(api_key="test-key")
            session = _wire_fake_session(provider, response)

            assert provider.available is True
            reply = await provider.ask("hi")

            assert reply == "ok", provider_cls.__name__
            call = session.calls[0]
            assert call["url"] == f"{base_url}/chat/completions", provider_cls.__name__
            assert call["headers"]["Authorization"] == "Bearer test-key", provider_cls.__name__
            assert call["json"]["model"] == default_model, provider_cls.__name__

    run(body())


def test_new_cloud_vendors_unavailable_without_api_key():
    for provider_cls in (QwenCloudProvider, OpenRouterProvider, TogetherProvider, FireworksProvider, DeepInfraProvider):
        assert provider_cls(api_key=None).available is False, provider_cls.__name__


def test_new_cloud_vendors_registered_in_factory():
    for vendor, provider_cls in (
        ("qwen", QwenCloudProvider),
        ("openrouter", OpenRouterProvider),
        ("together", TogetherProvider),
        ("fireworks", FireworksProvider),
        ("deepinfra", DeepInfraProvider),
    ):
        provider = create_provider({"provider": "cloud", "cloud_vendor": vendor, "api_key": "test-key"})
        assert isinstance(provider, provider_cls), vendor
        assert provider.available is True


# --- LocalAIProvider ------------------------------------------------


def test_local_provider_unavailable_without_url():
    provider = LocalAIProvider(base_url=None)
    assert provider.available is False


def test_local_provider_ask_uses_openai_compatible_shape_no_auth_header():
    async def body():
        response = _FakeResponse(200, {"choices": [{"message": {"content": "hello from qwen"}}]})
        provider = LocalAIProvider(base_url="http://192.168.1.50:11434/v1", model="qwen2.5")
        session = _wire_fake_session(provider, response)

        reply = await provider.ask("hi")

        assert reply == "hello from qwen"
        call = session.calls[0]
        assert call["url"] == "http://192.168.1.50:11434/v1/chat/completions"
        assert "Authorization" not in call["headers"]
        assert call["json"]["model"] == "qwen2.5"

    run(body())


def test_local_provider_accepts_an_uncensored_model_name_with_no_extra_code():
    # Qwen and an "uncensored"/abliterated tag both go through the same
    # class -- only the model string differs (see local.py docstring).
    provider = LocalAIProvider(base_url="http://192.168.1.50:11434/v1", model="dolphin-mixtral")
    assert provider.available is True
    assert provider._model == "dolphin-mixtral"


# --- credentials.py ---------------------------------------------------


def test_load_credentials_missing_file_returns_empty(tmp_path):
    result = credentials_module.load_credentials(tmp_path / "does-not-exist.json")
    assert result == {}


def test_save_then_load_credentials_roundtrip(tmp_path):
    path = tmp_path / "ai_credentials.json"
    credentials_module.save_credentials(
        {"provider": "cloud", "cloud_vendor": "anthropic", "api_key": "sk-ant-test", "model": "claude-x"},
        path,
    )

    loaded = credentials_module.load_credentials(path)

    assert loaded == {
        "provider": "cloud",
        "cloud_vendor": "anthropic",
        "api_key": "sk-ant-test",
        "model": "claude-x",
    }


def test_save_credentials_sets_owner_only_permissions(tmp_path):
    path = tmp_path / "ai_credentials.json"
    credentials_module.save_credentials({"provider": "local", "local_url": "http://192.168.1.50:11434/v1"}, path)

    mode = stat.S_IMODE(path.stat().st_mode)
    assert mode == stat.S_IRUSR | stat.S_IWUSR


def test_save_credentials_rejects_unknown_key(tmp_path):
    path = tmp_path / "ai_credentials.json"
    try:
        credentials_module.save_credentials({"provider": "local", "totally_made_up": "x"}, path)
    except ValueError:
        pass
    else:
        raise AssertionError("expected ValueError")
    assert not path.exists()


def test_load_credentials_ignores_unknown_keys_instead_of_failing(tmp_path):
    path = tmp_path / "ai_credentials.json"
    path.write_text('{"provider": "local", "local_url": "http://x", "junk": 1}', encoding="utf-8")

    loaded = credentials_module.load_credentials(path)

    assert loaded == {"provider": "local", "local_url": "http://x"}


def test_load_credentials_corrupt_json_returns_empty(tmp_path):
    path = tmp_path / "ai_credentials.json"
    path.write_text("{not valid json", encoding="utf-8")

    assert credentials_module.load_credentials(path) == {}


# --- factory.py ---------------------------------------------------


def test_factory_builds_cloud_provider_by_vendor():
    provider = create_provider(
        {"provider": "cloud", "cloud_vendor": "openai", "api_key": "sk-test", "model": "gpt-x"}
    )
    assert isinstance(provider, OpenAIProvider)
    assert provider.available is True


def test_factory_builds_local_provider():
    provider = create_provider({"provider": "local", "local_url": "http://192.168.1.50:11434/v1"})
    assert isinstance(provider, LocalAIProvider)
    assert provider.available is True


def test_factory_returns_unconfigured_when_nothing_stored():
    provider = create_provider({})
    assert provider.available is False


def test_factory_returns_unconfigured_for_unknown_cloud_vendor():
    provider = create_provider({"provider": "cloud", "cloud_vendor": "bing-chat", "api_key": "x"})
    assert provider.available is False


def test_factory_returns_unconfigured_for_non_string_cloud_vendor_instead_of_raising():
    # Regression: credentials can come straight from a client's JSON
    # body (rover_control/ai_panel.py) -- cloud_vendor being a
    # list/dict instead of a string used to raise an unhandled
    # TypeError from dict.get() on an unhashable key.
    provider = create_provider({"provider": "cloud", "cloud_vendor": ["not", "a", "string"], "api_key": "x"})
    assert provider.available is False


def test_factory_unconfigured_provider_ask_raises():
    async def body():
        provider = create_provider({})
        try:
            await provider.ask("hi")
        except AIProviderError:
            pass
        else:
            raise AssertionError("expected AIProviderError")

    run(body())
