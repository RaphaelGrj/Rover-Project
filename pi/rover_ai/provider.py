"""Common interface for rover-ai's interchangeable providers (cloud API
or local network LLM) -- ARCHITECTURE_AND_ROADMAP.md §17.1. Rover Core
(and, once Phase 7's audio pipeline exists) only ever talks to this
interface: it doesn't need to know or care which concrete provider is
active, same principle as rover_control.camera not knowing whether the
video feed used to come from picamera2 or now the ESP32-CAM.
"""
from __future__ import annotations

from dataclasses import dataclass, field


class AIProviderError(Exception):
    """A provider failed to produce a reply -- network error, HTTP
    error, malformed response, timeout, or simply not configured yet.
    Callers must catch this and fall back to degraded mode (§21: "un
    fournisseur IA indisponible ne doit jamais faire planter Rover ni
    bloquer le reste du robot"), never let it propagate unhandled."""


@dataclass
class AIContext:
    """Optional extra input alongside the raw message. `system` is a
    system-style instruction (Rover's persona/current state, once the
    personality engine exists -- §15); `history` is prior turns, oldest
    first, as plain (role, content) tuples with role "user"/"assistant"
    rather than provider-specific message objects, so callers building
    a conversation don't need to know any vendor's format either."""

    system: str | None = None
    history: list[tuple[str, str]] = field(default_factory=list)


class AIProvider:
    """Base class for every concrete provider (cloud vendor or local).

    `available` mirrors the "hardware/feature optional" pattern used
    elsewhere in this project (rover_control.camera.CameraStream,
    rover_mqtt.publisher.MqttPublisher): False when the provider is
    missing required configuration (no API key, no local address), so
    callers can check it before triggering a network call just to find
    out it was never going to work.
    """

    available: bool = False

    async def ask(self, message: str, context: AIContext | None = None) -> str:
        raise NotImplementedError

    async def close(self) -> None:
        """Release any held resources (HTTP session). No-op by default
        -- overridden by providers that actually open one."""
        return None
