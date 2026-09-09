"""Shared request/response shape for any OpenAI-compatible chat
completions endpoint. OpenAI's own API and Ollama's OpenAI-compatible
API (ARCHITECTURE_AND_ROADMAP.md §17.1: "l'API compatible OpenAI
d'Ollama") speak this exact shape, so cloud.OpenAIProvider and
local.LocalAIProvider share this base instead of duplicating it.
"""
from __future__ import annotations

from ._http import _HttpAIProvider
from .provider import AIContext, AIProviderError


class _OpenAICompatibleProvider(_HttpAIProvider):
    def __init__(self, base_url: str, model: str, api_key: str | None = None) -> None:
        super().__init__()
        self._base_url = base_url.rstrip("/")
        self._model = model
        self._api_key = api_key

    async def ask(self, message: str, context: AIContext | None = None) -> str:
        if not self.available:
            raise AIProviderError(f"{type(self).__name__} is not configured")
        context = context or AIContext()

        messages: list[dict[str, str]] = []
        if context.system:
            messages.append({"role": "system", "content": context.system})
        messages.extend({"role": role, "content": content} for role, content in context.history)
        messages.append({"role": "user", "content": message})

        headers = {"content-type": "application/json"}
        if self._api_key:
            headers["Authorization"] = f"Bearer {self._api_key}"

        data = await self._post_json(
            f"{self._base_url}/chat/completions",
            headers=headers,
            json_body={"model": self._model, "messages": messages},
        )
        try:
            return data["choices"][0]["message"]["content"]
        except (KeyError, IndexError, TypeError) as exc:
            raise AIProviderError(f"unexpected response shape from {self._base_url}: {data!r}") from exc
