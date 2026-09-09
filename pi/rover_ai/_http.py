"""Shared HTTP plumbing for every provider that talks to an HTTP API
(all of them so far -- cloud vendors and local Ollama-style servers
alike): session lifecycle plus a single place that turns network/HTTP
failures into AIProviderError, so each provider's ask() only has to
build a request body and parse a successful response.
"""
from __future__ import annotations

import asyncio
import json
import logging

import aiohttp

from .provider import AIProvider, AIProviderError

logger = logging.getLogger(__name__)

# Generous relative to a normal API call -- a cloud LLM reply or a
# local one running on modest hardware (a second Pi, per §17.1) can
# take a while, and a false timeout would misreport a slow-but-working
# provider as unavailable.
_CONNECT_TIMEOUT_S = 10
_READ_TIMEOUT_S = 30


class _HttpAIProvider(AIProvider):
    def __init__(self) -> None:
        self._session: aiohttp.ClientSession | None = None

    async def _get_session(self) -> aiohttp.ClientSession:
        # Created lazily, inside the running event loop that will
        # actually use it (aiohttp.ClientSession is unhappy about being
        # constructed outside one) -- same pattern as
        # rover_control.camera.CameraStream._get_session.
        if self._session is None or self._session.closed:
            self._session = aiohttp.ClientSession(
                timeout=aiohttp.ClientTimeout(connect=_CONNECT_TIMEOUT_S, sock_read=_READ_TIMEOUT_S)
            )
        return self._session

    async def close(self) -> None:
        if self._session is not None and not self._session.closed:
            await self._session.close()

    async def _post_json(self, url: str, *, headers: dict[str, str], json_body: dict) -> dict:
        session = await self._get_session()
        try:
            response = await session.post(url, headers=headers, json=json_body)
        except (aiohttp.ClientError, asyncio.TimeoutError) as exc:
            raise AIProviderError(f"network error contacting {url}: {exc}") from exc

        async with response:
            try:
                if response.status != 200:
                    body = await response.text()
                    # Truncated -- an error body can be an entire HTML
                    # page (a proxy/WAF block page, say), no need to
                    # log all of it.
                    raise AIProviderError(f"{url} returned HTTP {response.status}: {body[:200]!r}")
                return await response.json()
            except (aiohttp.ClientError, asyncio.TimeoutError) as exc:
                # Headers already came back fine (we're past the
                # session.post() await above) but the body read itself
                # failed -- a stalled/dropped connection mid-transfer
                # (sock_read timeout) or a wrong Content-Type
                # (aiohttp.ContentTypeError is a ClientError subclass).
                # Without this, either case raised unwrapped straight
                # out of ask(), bypassing the AIProviderError contract
                # every caller (ai_ask_post -> 503) relies on.
                raise AIProviderError(f"network error reading response from {url}: {exc}") from exc
            except json.JSONDecodeError as exc:
                raise AIProviderError(f"{url} returned malformed JSON: {exc}") from exc
