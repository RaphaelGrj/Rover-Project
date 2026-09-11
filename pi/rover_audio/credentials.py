"""Secret/config storage for rover_audio's STT and TTS provider
selection -- same principle as rover_ai.credentials (§17.1): never in
config.json (rover_core.config's own docstring says exactly this),
lives in its own git-ignored file with owner-only permissions. STT and
TTS are configured independently (each can pick its own vendor/key or
local address) but share one file -- one small future "Audio" panel,
same idea as rover-ai's single ai_credentials.json for its one provider
choice.
"""
from __future__ import annotations

import json
import logging
import os
import stat
from pathlib import Path
from typing import Any

logger = logging.getLogger(__name__)

DEFAULT_CREDENTIALS_PATH = Path(__file__).resolve().parent.parent / "audio_credentials.json"

# {stt,tts}_provider: "cloud" | "local". {stt,tts}_cloud_vendor (only
# meaningful when the matching provider == "cloud"): "openai" for now --
# see rover_audio/factory.py's _CLOUD_STT_VENDORS/_CLOUD_TTS_VENDORS for
# the authoritative list. model/api_key/local_url/voice are all
# optional -- each provider class fills in its own default, an
# unconfigured one just means `available` stays False.
_ALLOWED_KEYS = {
    "stt_provider", "stt_cloud_vendor", "stt_api_key", "stt_model", "stt_local_url",
    "tts_provider", "tts_cloud_vendor", "tts_api_key", "tts_model", "tts_voice", "tts_local_url",
}


def load_credentials(path: Path | None = None) -> dict[str, Any]:
    """Returns {} if the file doesn't exist or can't be parsed -- a
    missing/corrupt credentials file must never crash Rover, just leave
    rover_audio unconfigured (mirrors rover_ai.credentials.load_credentials)."""
    target = path or DEFAULT_CREDENTIALS_PATH
    if not target.exists():
        return {}

    try:
        with target.open(encoding="utf-8") as f:
            data = json.load(f)
    except (OSError, json.JSONDecodeError) as exc:
        logger.warning("could not read %s, rover_audio stays unconfigured: %s", target, exc)
        return {}

    if not isinstance(data, dict):
        logger.warning("%s does not contain a JSON object, ignoring", target)
        return {}

    unknown = set(data) - _ALLOWED_KEYS
    if unknown:
        logger.warning("ignoring unknown key(s) in %s: %s", target, sorted(unknown))
    return {k: v for k, v in data.items() if k in _ALLOWED_KEYS}


def save_credentials(data: dict[str, Any], path: Path | None = None) -> None:
    """Writes atomically (tmp file + rename) with owner-read/write-only
    permissions set at creation time -- same approach as
    rover_ai.credentials.save_credentials, see its docstring for why."""
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
