"""LocalAIProvider -- ARCHITECTURE_AND_ROADMAP.md §17.1: a second
Raspberry Pi (or any machine) on the same local network, serving an
OpenAI-compatible API. Ollama is the reference target named in the
architecture doc, but nothing here is Ollama-specific.

Qwen 2.5 (the doc's own example) and an uncensored fine-tune are both
just a `model` string -- Ollama (or any compatible server) resolves
the model name, this class never needs to know what's behind it. No
extra provider code per model, same way OpenAIProvider doesn't need a
subclass per GPT variant.
"""
from __future__ import annotations

from ._openai_compatible import _OpenAICompatibleProvider


class LocalAIProvider(_OpenAICompatibleProvider):
    DEFAULT_MODEL = "qwen2.5"

    def __init__(self, base_url: str | None, model: str | None = None) -> None:
        super().__init__(base_url or "", model or self.DEFAULT_MODEL, api_key=None)
        # No API key required for a trusted local network server -- see
        # §17.1 ("aucune clé requise, juste une adresse réseau").
        self.available = bool(base_url)
