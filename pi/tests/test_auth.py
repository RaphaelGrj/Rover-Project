"""Tests for rover_control.auth -- the access-token gate in front of
the whole control server (see auth.py's module docstring for why this
matters for an open-source project people run unmodified)."""
from __future__ import annotations

from rover_control.auth import resolve_token, token_matches


def test_resolve_token_uses_environment_variable_when_set(monkeypatch):
    monkeypatch.setenv("ROVER_CONTROL_TOKEN", "my-fixed-token")
    assert resolve_token() == "my-fixed-token"


def test_resolve_token_generates_a_random_one_when_unset(monkeypatch):
    monkeypatch.delenv("ROVER_CONTROL_TOKEN", raising=False)
    first = resolve_token()
    second = resolve_token()
    # Never the same value twice -- a "generated once, reused forever"
    # fallback would be almost as bad as a hardcoded default.
    assert first != second
    # secrets.token_urlsafe(24) -- long enough that brute-forcing it
    # over a control server is not a realistic concern.
    assert len(first) > 24


def test_token_matches_accepts_the_correct_token():
    assert token_matches("abc123", "abc123") is True


def test_token_matches_rejects_wrong_token():
    assert token_matches("wrong", "abc123") is False


def test_token_matches_rejects_missing_token():
    assert token_matches(None, "abc123") is False


def test_token_matches_rejects_empty_string():
    # An empty ?token= must not accidentally match an empty expected
    # value -- resolve_token() never returns an empty string in
    # practice, but the comparison itself shouldn't rely on that.
    assert token_matches("", "") is False
