"""Tests for rover_control.ai_panel.AIPanel -- the logic behind the
rover-ai config/test-chat routes (rover_control/server.py's /ai/*
routes). The routes themselves stay untested, same "business logic
tested, raw aiohttp wiring isn't" split already used for
camera.py/server.py in this project (see PROGRESS.md). create_provider
is monkeypatched here so these tests never build a real network-backed
provider, same spirit as test_mqtt_publisher.py's fake-import pattern.
"""
from __future__ import annotations

import asyncio

import rover_control.ai_panel as ai_panel_module
from rover_ai.provider import AIProvider, AIProviderError
from rover_control.ai_panel import AIPanel, REDACTED_API_KEY


def run(coro):
    return asyncio.run(coro)


class FakeProvider(AIProvider):
    def __init__(self, available: bool = True, reply: str = "fake reply") -> None:
        self.available = available
        self._reply = reply
        self.closed = False
        self.asked_with: str | None = None

    async def ask(self, message, context=None):
        self.asked_with = message
        if not self.available:
            raise AIProviderError("not configured")
        return self._reply

    async def close(self):
        self.closed = True


def _stub_create_provider(monkeypatch, factory=None):
    built_with: list[dict] = []

    def fake_create_provider(credentials):
        built_with.append(dict(credentials))
        return factory(credentials) if factory else FakeProvider()

    monkeypatch.setattr(ai_panel_module, "create_provider", fake_create_provider)
    return built_with


def test_public_config_redacts_api_key(tmp_path, monkeypatch):
    _stub_create_provider(monkeypatch)
    path = tmp_path / "ai_credentials.json"
    path.write_text(
        '{"provider": "cloud", "cloud_vendor": "openai", "api_key": "sk-real-secret", "model": "gpt-x"}',
        encoding="utf-8",
    )

    panel = AIPanel(path)
    config = panel.public_config()

    assert config["api_key"] == REDACTED_API_KEY
    assert config["provider"] == "cloud"
    assert config["cloud_vendor"] == "openai"
    assert config["available"] is True


def test_public_config_when_unconfigured(tmp_path, monkeypatch):
    _stub_create_provider(monkeypatch, factory=lambda creds: FakeProvider(available=False))
    panel = AIPanel(tmp_path / "does-not-exist.json")

    config = panel.public_config()

    assert "api_key" not in config
    assert config["available"] is False


def test_update_saves_and_rebuilds_provider(tmp_path, monkeypatch):
    built_with = _stub_create_provider(monkeypatch)
    path = tmp_path / "ai_credentials.json"

    async def body():
        panel = AIPanel(path)
        await panel.update({"provider": "local", "local_url": "http://192.168.1.50:11434/v1", "model": "qwen2.5"})

    run(body())

    expected = {"provider": "local", "local_url": "http://192.168.1.50:11434/v1", "model": "qwen2.5"}
    assert built_with[-1] == expected
    assert ai_panel_module.load_credentials(path) == expected


def test_update_ignores_unknown_fields(tmp_path, monkeypatch):
    _stub_create_provider(monkeypatch)
    path = tmp_path / "ai_credentials.json"

    async def body():
        panel = AIPanel(path)
        await panel.update({"provider": "local", "local_url": "http://x", "totally_made_up": "x"})

    run(body())

    assert "totally_made_up" not in ai_panel_module.load_credentials(path)


def test_update_keeps_existing_api_key_when_field_left_blank(tmp_path, monkeypatch):
    _stub_create_provider(monkeypatch)
    path = tmp_path / "ai_credentials.json"
    path.write_text(
        '{"provider": "cloud", "cloud_vendor": "anthropic", "api_key": "sk-real-secret"}', encoding="utf-8"
    )

    async def body():
        panel = AIPanel(path)
        await panel.update({"provider": "cloud", "cloud_vendor": "anthropic", "api_key": ""})

    run(body())

    assert ai_panel_module.load_credentials(path)["api_key"] == "sk-real-secret"


def test_update_keeps_existing_api_key_when_field_is_the_redacted_placeholder(tmp_path, monkeypatch):
    _stub_create_provider(monkeypatch)
    path = tmp_path / "ai_credentials.json"
    path.write_text(
        '{"provider": "cloud", "cloud_vendor": "anthropic", "api_key": "sk-real-secret"}', encoding="utf-8"
    )

    async def body():
        panel = AIPanel(path)
        await panel.update({"provider": "cloud", "cloud_vendor": "anthropic", "api_key": REDACTED_API_KEY})

    run(body())

    assert ai_panel_module.load_credentials(path)["api_key"] == "sk-real-secret"


def test_update_does_not_carry_over_the_old_vendors_key_when_switching_cloud_vendor(tmp_path, monkeypatch):
    # Regression: leaving the API key field blank while switching from
    # one cloud vendor to another used to silently keep the OLD
    # vendor's secret and save it as the NEW vendor's key -- the next
    # /ai/ask would then send eg. an Anthropic key as the OpenAI Bearer
    # token, leaking the credential to the wrong service.
    _stub_create_provider(monkeypatch)
    path = tmp_path / "ai_credentials.json"
    path.write_text(
        '{"provider": "cloud", "cloud_vendor": "anthropic", "api_key": "sk-ant-real-secret"}', encoding="utf-8"
    )

    async def body():
        panel = AIPanel(path)
        await panel.update({"provider": "cloud", "cloud_vendor": "openai", "api_key": ""})

    run(body())

    saved = ai_panel_module.load_credentials(path)
    assert "api_key" not in saved
    assert saved["cloud_vendor"] == "openai"


def test_update_keeps_a_freshly_typed_key_even_when_switching_cloud_vendor(tmp_path, monkeypatch):
    # The blank-field/vendor-change guard above must not swallow a real,
    # explicitly-typed new key just because the vendor also changed.
    _stub_create_provider(monkeypatch)
    path = tmp_path / "ai_credentials.json"
    path.write_text(
        '{"provider": "cloud", "cloud_vendor": "anthropic", "api_key": "sk-ant-real-secret"}', encoding="utf-8"
    )

    async def body():
        panel = AIPanel(path)
        await panel.update({"provider": "cloud", "cloud_vendor": "openai", "api_key": "sk-openai-new-secret"})

    run(body())

    saved = ai_panel_module.load_credentials(path)
    assert saved["api_key"] == "sk-openai-new-secret"


def test_update_replaces_stale_fields_when_switching_provider_type(tmp_path, monkeypatch):
    _stub_create_provider(monkeypatch)
    path = tmp_path / "ai_credentials.json"
    path.write_text(
        '{"provider": "cloud", "cloud_vendor": "openai", "api_key": "sk-real-secret", "model": "gpt-x"}',
        encoding="utf-8",
    )

    async def body():
        panel = AIPanel(path)
        await panel.update({"provider": "local", "local_url": "http://192.168.1.50:11434/v1"})

    run(body())

    assert ai_panel_module.load_credentials(path) == {
        "provider": "local",
        "local_url": "http://192.168.1.50:11434/v1",
    }


def test_ask_delegates_to_active_provider(tmp_path, monkeypatch):
    fake = FakeProvider(reply="hello")
    _stub_create_provider(monkeypatch, factory=lambda creds: fake)
    path = tmp_path / "ai_credentials.json"

    async def body():
        panel = AIPanel(path)
        reply = await panel.ask("hi there")
        assert reply == "hello"
        assert fake.asked_with == "hi there"

    run(body())


def test_ask_propagates_provider_error(tmp_path, monkeypatch):
    _stub_create_provider(monkeypatch, factory=lambda creds: FakeProvider(available=False))
    path = tmp_path / "ai_credentials.json"

    async def body():
        panel = AIPanel(path)
        try:
            await panel.ask("hi")
        except AIProviderError:
            pass
        else:
            raise AssertionError("expected AIProviderError")

    run(body())


def test_close_closes_active_provider(tmp_path, monkeypatch):
    fake = FakeProvider()
    _stub_create_provider(monkeypatch, factory=lambda creds: fake)
    path = tmp_path / "ai_credentials.json"

    async def body():
        panel = AIPanel(path)
        await panel.close()
        assert fake.closed is True

    run(body())


def test_update_closes_the_previous_provider_before_building_the_new_one(tmp_path, monkeypatch):
    first = FakeProvider()
    providers = [first, FakeProvider()]
    _stub_create_provider(monkeypatch, factory=lambda creds: providers.pop(0))
    path = tmp_path / "ai_credentials.json"

    async def body():
        panel = AIPanel(path)
        await panel.update({"provider": "local", "local_url": "http://x"})
        assert first.closed is True

    run(body())
