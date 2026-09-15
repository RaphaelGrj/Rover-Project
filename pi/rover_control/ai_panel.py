"""Web panel logic for rover-ai (ARCHITECTURE_AND_ROADMAP.md §17.1):
lets the operator pick a provider and test it with a text message from
the same authenticated web app as the rest of rover_control -- no
redeploy needed to switch providers or update a key, same
"reconfigure without reflash" idea as
esp32/lib/network/RoverWifiProvisioning.h, just on the Pi side.

Kept separate from server.py's route handlers on purpose: this class
holds all the actual logic (config read/write, provider rebuild, the
"blank field keeps the existing key" convention) and is unit-tested
directly (pi/tests/test_ai_panel.py); the aiohttp routes in server.py
stay thin wiring around it, same "business logic tested, raw web layer
isn't" split already used for camera.py/server.py in this project.
"""
from __future__ import annotations

from pathlib import Path
from typing import Any

from rover_ai import create_provider, load_credentials, save_credentials
from rover_ai.credentials import DEFAULT_CREDENTIALS_PATH
from rover_ai.personality import PersonalityEngine

# The only fields a client is ever allowed to read or write -- anything
# else in an incoming request body is silently dropped rather than
# rejected, same permissive-but-safe stance as
# rover_core.config.load_config's "ignoring unknown config.json key(s)".
CONFIG_FIELDS = ("provider", "cloud_vendor", "api_key", "model", "local_url")

# Returned to the browser instead of the real key -- confirms one is
# set without echoing it back to whatever's looking at the screen or a
# network trace. Redacting rather than omitting the field keeps the
# response shape identical either way, simpler for the frontend to render.
REDACTED_API_KEY = "********"


class AIPanel:
    def __init__(
        self,
        credentials_path: Path | None = None,
        personality: PersonalityEngine | None = None,
    ) -> None:
        self._path = credentials_path or DEFAULT_CREDENTIALS_PATH
        self._credentials = load_credentials(self._path)
        self._provider = create_provider(self._credentials)
        # Optional: main.py wires this to RoverCore.set_emotion so a
        # conversation turn also shows on Rover's face (§15). Defaults to
        # a sink-less engine (still gives every conversation persona +
        # history, just no emotion reaction) so tests/callers that don't
        # care about RoverCore keep working unchanged.
        self._personality = personality or PersonalityEngine()

    def public_config(self) -> dict[str, Any]:
        data = {k: v for k, v in self._credentials.items() if k in CONFIG_FIELDS}
        if data.get("api_key"):
            data["api_key"] = REDACTED_API_KEY
        data["available"] = self._provider.available
        return data

    async def update(self, new_data: dict[str, Any]) -> None:
        """Replaces the stored config with `new_data` (only the known
        CONFIG_FIELDS are kept, everything else ignored) and rebuilds
        the active provider from it. A full replace, not a merge --
        the settings form always submits the complete desired state,
        so switching eg. cloud -> local correctly drops the now-stale
        cloud_vendor/api_key instead of leaving them behind unused."""
        new_data = {k: new_data.get(k) for k in CONFIG_FIELDS if k in new_data}

        api_key = new_data.get("api_key")
        blank_or_placeholder = not api_key or api_key == REDACTED_API_KEY

        if new_data.get("provider") != "cloud":
            # Not a cloud provider at all -- api_key is meaningless
            # here, drop it outright rather than letting the "keep
            # existing" logic below carry a stale cloud secret forward
            # into a local/unconfigured entry.
            new_data.pop("api_key", None)
        elif blank_or_placeholder:
            # Blank or still showing the placeholder from
            # public_config() -- keep whatever key was already stored
            # (same "blank field keeps existing value" convention as
            # the ESP32 WiFi portal's OTA password field), but ONLY if
            # the vendor didn't also change: a stored Anthropic key
            # left over from before must never be silently reused as
            # the OpenAI key just because the field was left blank
            # while switching vendors -- that would send the wrong
            # secret to the wrong third-party service.
            same_vendor = new_data.get("cloud_vendor") == self._credentials.get("cloud_vendor")
            existing = self._credentials.get("api_key") if same_vendor else None
            if existing:
                new_data["api_key"] = existing
            else:
                new_data.pop("api_key", None)
        # else: a real new key was typed in -- keep it as given,
        # whether or not the vendor also changed.

        cleaned = {k: v for k, v in new_data.items() if v not in (None, "")}

        # Save FIRST, then swap the provider. The other order (close,
        # then save) leaves the panel holding a closed provider if
        # save_credentials() raises -- every later ask() would fail on a
        # closed session, with a restart as the only way out. Saving
        # first means a failed write changes nothing at all.
        save_credentials(cleaned, self._path)
        await self._provider.close()
        self._credentials = cleaned
        self._provider = create_provider(cleaned)
        # A reconfigured provider/vendor should not silently carry over
        # history built against a different backend (§17.1).
        self._personality.reset()

    async def ask(self, message: str) -> str:
        return await self._personality.converse(self._provider.ask, message)

    def reset_conversation(self) -> None:
        self._personality.reset()

    async def close(self) -> None:
        await self._provider.close()
