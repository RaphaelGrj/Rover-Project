"""rover-ai -- interchangeable AI provider (cloud API or local network
LLM), see ARCHITECTURE_AND_ROADMAP.md §17.1. Public surface: build the
active provider from stored credentials, then call .ask() -- callers
never need to import a specific vendor class.
"""
from __future__ import annotations

from .credentials import load_credentials, save_credentials
from .factory import create_provider
from .provider import AIContext, AIProvider, AIProviderError

__all__ = [
    "AIContext",
    "AIProvider",
    "AIProviderError",
    "create_provider",
    "load_credentials",
    "save_credentials",
]
