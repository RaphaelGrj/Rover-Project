"""Access-token authentication for the control server.

This project is open source and gets run by people who downloaded it
from GitHub, on their own local network. Rover's control page can drive
the physical robot and (once wired up) see a video feed from it --
serving that with zero authentication by default would mean anyone else
on the same WiFi/LAN can too. "Secure by default, the operator can only
make it *more* open" is the safer default for code other people run
unmodified, versus "open by default, remember to lock it down" which
depends on every downstream user remembering a step they're unlikely to
even think of.

The token itself is never hardcoded or committed -- a shared default
baked into a public repo would defeat the entire point, since everyone
who didn't change it would have the same one. It's read from
ROVER_CONTROL_TOKEN if set; otherwise a random one is generated fresh
for this run only and printed to the log, so a first run is still
usable without editing any file, and stays unpredictable to anyone who
hasn't read this process's own console output.
"""
from __future__ import annotations

import hmac
import logging
import os
import secrets

logger = logging.getLogger(__name__)


def resolve_token() -> str:
    token = os.environ.get("ROVER_CONTROL_TOKEN")
    if token:
        return token
    token = secrets.token_urlsafe(24)
    logger.warning(
        "ROVER_CONTROL_TOKEN is not set -- generated a random token for THIS RUN ONLY "
        "(it will be different next time you start this): %s\n"
        "Open the control page at http://<host>:<port>/?token=%s\n"
        "Set the ROVER_CONTROL_TOKEN environment variable yourself to keep a stable link "
        "across restarts -- see pi/README.md.",
        token, token,
    )
    return token


def token_matches(candidate: str | None, expected: str) -> bool:
    """Constant-time comparison -- avoids leaking anything about the
    token through response-time differences. A minor concern for a
    single self-hosted instance, but this code is run by people who
    didn't write it and may end up exposed more broadly than intended
    (eg. port-forwarded before the VPN work in Phase 6 exists), so it
    costs nothing to not cut this corner."""
    if candidate is None:
        return False
    return hmac.compare_digest(candidate, expected)
