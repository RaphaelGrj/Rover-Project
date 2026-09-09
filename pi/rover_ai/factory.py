"""Picks the concrete AIProvider from stored credentials
(rover_ai.credentials) -- the one place that knows about every vendor,
so callers (eventually rover_core, once Phase 7 wires this in) never
import a specific provider class directly. Same decoupling as
CameraStream hiding picamera2-vs-ESP32-CAM from rover_control.server.
"""
from __future__ import annotations

import logging
from typing import Any

from .cloud import (
    AnthropicProvider,
    DeepInfraProvider,
    FireworksProvider,
    GeminiProvider,
    OpenAIProvider,
    OpenRouterProvider,
    QwenCloudProvider,
    TogetherProvider,
)
from .local import LocalAIProvider
from .provider import AIContext, AIProvider, AIProviderError

logger = logging.getLogger(__name__)

_CLOUD_VENDORS = {
    "anthropic": AnthropicProvider,
    "openai": OpenAIProvider,
    "gemini": GeminiProvider,
    # OpenAI-compatible aggregators (cloud.py's "OpenAI-compatible
    # aggregators" section) -- Qwen's own cloud API, plus hosts of
    # community fine-tunes including uncensored ones.
    "qwen": QwenCloudProvider,
    "openrouter": OpenRouterProvider,
    "together": TogetherProvider,
    "fireworks": FireworksProvider,
    "deepinfra": DeepInfraProvider,
}


class _UnconfiguredProvider(AIProvider):
    """Returned when nothing is configured yet, or configured with an
    unrecognized vendor -- available=False like every other provider,
    so a caller that checks it first never even reaches ask(). ask()
    itself still raises AIProviderError rather than returning an empty
    string, so a caller that skips the check gets a clear failure
    instead of a silently blank reply."""

    available = False

    async def ask(self, message: str, context: AIContext | None = None) -> str:
        raise AIProviderError("rover-ai is not configured yet")


def create_provider(credentials: dict[str, Any]) -> AIProvider:
    provider_kind = credentials.get("provider")

    if provider_kind == "cloud":
        vendor = credentials.get("cloud_vendor")
        # isinstance guard first -- credentials can come straight from
        # a client's JSON body (rover_control/ai_panel.py), where
        # cloud_vendor could be a list/dict/etc. instead of a string.
        # dict.get() on an unhashable value raises TypeError, which
        # would otherwise escape as an unhandled 500 instead of the
        # intended "unknown vendor -> unconfigured" graceful path.
        cls = _CLOUD_VENDORS.get(vendor) if isinstance(vendor, str) else None
        if cls is None:
            logger.warning("unknown or missing cloud_vendor %r -- rover-ai stays unconfigured", vendor)
            return _UnconfiguredProvider()
        return cls(credentials.get("api_key"), credentials.get("model"))

    if provider_kind == "local":
        return LocalAIProvider(credentials.get("local_url"), credentials.get("model"))

    if provider_kind is not None:
        logger.warning("unknown rover-ai provider kind %r -- staying unconfigured", provider_kind)
    return _UnconfiguredProvider()
