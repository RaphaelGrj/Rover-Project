"""Cloud API providers -- ARCHITECTURE_AND_ROADMAP.md §17.1
CloudAIProvider family. Each wraps one vendor's HTTP API behind the
same AIProvider.ask() interface; picking which one is active is a
runtime config choice (rover_ai.credentials / factory), never a
build-time one -- none of this is imported conditionally on any flag.
"""
from __future__ import annotations

from ._http import _HttpAIProvider
from ._openai_compatible import _OpenAICompatibleProvider
from .provider import AIContext, AIProviderError


class AnthropicProvider(_HttpAIProvider):
    """Anthropic Messages API (api.anthropic.com) -- distinct request
    shape from the OpenAI-compatible family (system prompt is a
    top-level field, not a "system" message; response text sits under
    content[0].text instead of choices[0].message.content)."""

    DEFAULT_MODEL = "claude-sonnet-4-5"

    def __init__(self, api_key: str | None, model: str | None = None) -> None:
        super().__init__()
        self._api_key = api_key
        self._model = model or self.DEFAULT_MODEL
        self.available = bool(api_key)

    async def ask(self, message: str, context: AIContext | None = None) -> str:
        if not self.available:
            raise AIProviderError("AnthropicProvider is not configured (missing API key)")
        context = context or AIContext()

        messages = [{"role": role, "content": content} for role, content in context.history]
        messages.append({"role": "user", "content": message})

        body: dict = {"model": self._model, "max_tokens": 1024, "messages": messages}
        if context.system:
            body["system"] = context.system

        data = await self._post_json(
            "https://api.anthropic.com/v1/messages",
            headers={
                "x-api-key": self._api_key,
                "anthropic-version": "2023-06-01",
                "content-type": "application/json",
            },
            json_body=body,
        )
        try:
            return data["content"][0]["text"]
        except (KeyError, IndexError, TypeError) as exc:
            raise AIProviderError(f"unexpected Anthropic response shape: {data!r}") from exc


class OpenAIProvider(_OpenAICompatibleProvider):
    """OpenAI Chat Completions API (api.openai.com)."""

    DEFAULT_MODEL = "gpt-4o-mini"

    def __init__(self, api_key: str | None, model: str | None = None) -> None:
        super().__init__("https://api.openai.com/v1", model or self.DEFAULT_MODEL, api_key)
        self.available = bool(api_key)


class GeminiProvider(_HttpAIProvider):
    """Google Gemini generateContent API -- yet another distinct shape
    (contents[].parts[].text, role "model" instead of "assistant")."""

    DEFAULT_MODEL = "gemini-2.5-flash"

    def __init__(self, api_key: str | None, model: str | None = None) -> None:
        super().__init__()
        self._api_key = api_key
        self._model = model or self.DEFAULT_MODEL
        self.available = bool(api_key)

    async def ask(self, message: str, context: AIContext | None = None) -> str:
        if not self.available:
            raise AIProviderError("GeminiProvider is not configured (missing API key)")
        context = context or AIContext()

        contents = [
            {"role": "user" if role == "user" else "model", "parts": [{"text": content}]}
            for role, content in context.history
        ]
        contents.append({"role": "user", "parts": [{"text": message}]})

        body: dict = {"contents": contents}
        if context.system:
            body["systemInstruction"] = {"parts": [{"text": context.system}]}

        url = f"https://generativelanguage.googleapis.com/v1beta/models/{self._model}:generateContent"
        data = await self._post_json(
            url,
            headers={"x-goog-api-key": self._api_key, "content-type": "application/json"},
            json_body=body,
        )
        try:
            return data["candidates"][0]["content"]["parts"][0]["text"]
        except (KeyError, IndexError, TypeError) as exc:
            raise AIProviderError(f"unexpected Gemini response shape: {data!r}") from exc


# --- OpenAI-compatible aggregators -------------------------------------
#
# These five all speak the exact same request/response shape as
# OpenAIProvider above (that's the point of an "OpenAI-compatible"
# endpoint) -- each class below is only a base_url + a default model,
# no new request/parsing logic. Requested by the user (2026-09-09) as
# ways to run Qwen or an uncensored model in the cloud instead of
# self-hosting via LocalAIProvider/Ollama.


class QwenCloudProvider(_OpenAICompatibleProvider):
    """Qwen's own cloud API -- Alibaba Cloud DashScope's OpenAI-compatible
    endpoint, i.e. Qwen as a paid cloud service instead of self-hosted
    via LocalAIProvider/Ollama (§17.1's own example). International
    endpoint by default; DashScope also has a mainland-China-only
    endpoint (dashscope.aliyuncs.com) not covered here."""

    DEFAULT_MODEL = "qwen-plus"

    def __init__(self, api_key: str | None, model: str | None = None) -> None:
        super().__init__(
            "https://dashscope-intl.aliyuncs.com/compatible-mode/v1",
            model or self.DEFAULT_MODEL,
            api_key,
        )
        self.available = bool(api_key)


class OpenRouterProvider(_OpenAICompatibleProvider):
    """OpenRouter -- aggregator routing to many hosted models under one
    API/key, including several community "uncensored"/lightly-moderated
    fine-tunes (Dolphin, abliterated variants, ...). Its catalog rotates
    -- verify the exact model id at openrouter.ai/models before relying
    on it in production; the default below is only a safe fallback so
    this class isn't unusable out of the box, not a recommendation of
    which model to actually run."""

    DEFAULT_MODEL = "meta-llama/llama-3.1-8b-instruct"

    def __init__(self, api_key: str | None, model: str | None = None) -> None:
        super().__init__("https://openrouter.ai/api/v1", model or self.DEFAULT_MODEL, api_key)
        self.available = bool(api_key)


class TogetherProvider(_OpenAICompatibleProvider):
    """Together.ai -- hosts Qwen (default below) alongside many
    community fine-tunes including uncensored ones (Dolphin, Hermes) --
    set `model` to whichever one you pick, same class either way."""

    DEFAULT_MODEL = "Qwen/Qwen2.5-72B-Instruct-Turbo"

    def __init__(self, api_key: str | None, model: str | None = None) -> None:
        super().__init__("https://api.together.xyz/v1", model or self.DEFAULT_MODEL, api_key)
        self.available = bool(api_key)


class FireworksProvider(_OpenAICompatibleProvider):
    """Fireworks.ai -- same idea as Together, different catalog and a
    different model id format (accounts/fireworks/models/<name>)."""

    DEFAULT_MODEL = "accounts/fireworks/models/qwen2p5-72b-instruct"

    def __init__(self, api_key: str | None, model: str | None = None) -> None:
        super().__init__("https://api.fireworks.ai/inference/v1", model or self.DEFAULT_MODEL, api_key)
        self.available = bool(api_key)


class DeepInfraProvider(_OpenAICompatibleProvider):
    """DeepInfra -- another OpenAI-compatible host for Qwen and
    community fine-tunes. Pricing/catalog specifics move over time --
    verify at deepinfra.com/models before assuming any given model is
    still listed."""

    DEFAULT_MODEL = "Qwen/Qwen2.5-72B-Instruct"

    def __init__(self, api_key: str | None, model: str | None = None) -> None:
        super().__init__("https://api.deepinfra.com/v1/openai", model or self.DEFAULT_MODEL, api_key)
        self.available = bool(api_key)
