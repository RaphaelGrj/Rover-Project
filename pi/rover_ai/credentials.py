"""Secret storage for rover-ai's active provider selection and API
key/local address -- same principle as ROVER_CONTROL_TOKEN
(rover_control/auth.py) and esp32/lib/network/WifiCredentialsStore.h:
never in config.json (rover_core/config.py's own docstring says exactly
this), lives in its own git-ignored file with owner-only permissions.
Written by the config panel once it exists (ARCHITECTURE_AND_ROADMAP.md
§17.1); this module is the storage layer underneath it.
"""
from __future__ import annotations

import json
import logging
import os
import stat
from pathlib import Path
from typing import Any

logger = logging.getLogger(__name__)

DEFAULT_CREDENTIALS_PATH = Path(__file__).resolve().parent.parent / "ai_credentials.json"

# provider: "cloud" | "local". cloud_vendor (only meaningful when
# provider == "cloud"): "anthropic" | "openai" | "gemini" | "qwen" |
# "openrouter" | "together" | "fireworks" | "deepinfra" -- see
# rover_ai/factory.py's _CLOUD_VENDORS for the authoritative list.
# model/api_key/local_url are all optional -- each provider class fills
# in its own default model, and local_url-less/api_key-less just means
# `available` stays False (see AIProvider docstring).
_ALLOWED_KEYS = {"provider", "cloud_vendor", "api_key", "model", "local_url"}


def load_credentials(path: Path | None = None) -> dict[str, Any]:
    """Returns {} if the file doesn't exist or can't be parsed -- a
    missing/corrupt credentials file must never crash Rover, just leave
    rover-ai unconfigured (mirrors rover_core.config.load_config's
    "missing file is not an error" stance)."""
    target = path or DEFAULT_CREDENTIALS_PATH
    if not target.exists():
        return {}

    try:
        with target.open(encoding="utf-8") as f:
            data = json.load(f)
    except (OSError, json.JSONDecodeError) as exc:
        logger.warning("could not read %s, rover-ai stays unconfigured: %s", target, exc)
        return {}

    if not isinstance(data, dict):
        logger.warning("%s does not contain a JSON object, ignoring", target)
        return {}

    unknown = set(data) - _ALLOWED_KEYS
    if unknown:
        logger.warning("ignoring unknown key(s) in %s: %s", target, sorted(unknown))
    return {k: v for k, v in data.items() if k in _ALLOWED_KEYS}


def save_credentials(data: dict[str, Any], path: Path | None = None) -> None:
    """Writes atomically (tmp file + rename, so a process crash mid-write
    can never leave a half-written/corrupt credentials file) with
    owner-read/write-only permissions set at creation time via os.open's
    mode argument -- avoids the brief window a plain open() + chmod()
    would leave where the file exists world-readable before it's
    restricted."""
    unknown = set(data) - _ALLOWED_KEYS
    if unknown:
        raise ValueError(f"unknown credential key(s): {sorted(unknown)}")

    target = path or DEFAULT_CREDENTIALS_PATH
    target.parent.mkdir(parents=True, exist_ok=True)
    tmp_path = target.with_suffix(".tmp")

    fd = os.open(tmp_path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, stat.S_IRUSR | stat.S_IWUSR)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)
    except BaseException:
        tmp_path.unlink(missing_ok=True)
        raise
    tmp_path.replace(target)
