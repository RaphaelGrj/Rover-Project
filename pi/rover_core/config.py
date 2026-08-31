"""Optional JSON config file, layered under CLI args (an explicitly
given CLI arg always wins). Keeps the common case
(`python -m rover_core.main --port ...`) exactly as simple as before,
while giving a documented place to set defaults once instead of
retyping them -- and a path towards a config-only invocation (a systemd
unit, say) later without touching argparse.

Never put secrets here: config.json is loaded from disk in plain text
and is git-ignored by convention (like any local override), but that's
not the same guarantee as an environment variable -- the control
server's access token (rover_control/auth.py) deliberately does NOT
live here.
"""
from __future__ import annotations

import json
import logging
from pathlib import Path
from typing import Any

logger = logging.getLogger(__name__)

DEFAULT_CONFIG_PATH = Path(__file__).resolve().parent.parent / "config.json"

DEFAULTS: dict[str, Any] = {
    "port": "/dev/ttyUSB0",
    "baudrate": 115200,
    "http_host": "0.0.0.0",
    "http_port": 8080,
    "log_level": "INFO",
    "log_dir": None,  # None = console only; set to enable rotating file logs
}


def load_config(path: Path | None = None) -> dict[str, Any]:
    """Returns DEFAULTS merged with the JSON file at `path` (or
    DEFAULT_CONFIG_PATH if `path` wasn't given and that default file
    happens to exist). A missing file is not an error -- config.json is
    entirely optional, CLI args alone still work exactly as before."""
    config = dict(DEFAULTS)
    target = path or DEFAULT_CONFIG_PATH

    if not target.exists():
        if path is not None:
            logger.warning("config file %s not found, using defaults", path)
        return config

    try:
        with target.open(encoding="utf-8") as f:
            data = json.load(f)
    except (OSError, json.JSONDecodeError) as exc:
        logger.warning("could not read %s, using defaults: %s", target, exc)
        return config

    unknown = set(data) - set(DEFAULTS)
    if unknown:
        logger.warning("ignoring unknown config.json key(s): %s", sorted(unknown))
    config.update({k: v for k, v in data.items() if k in DEFAULTS})
    return config
