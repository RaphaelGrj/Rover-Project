"""Tests for rover_ai.personality.PersonalityEngine -- the persona/history
builder and emotion reaction wired into rover_control.ai_panel.AIPanel.ask()
(see test_ai_panel.py for the wiring itself). Uses a fake `ask` callable
instead of a real AIProvider -- this module never talks to a provider
directly, same "business logic tested in isolation" split as the rest of
pi/.
"""
from __future__ import annotations

import asyncio

from rover_ai.personality import MAX_HISTORY_TURNS, PersonalityEngine
from rover_ai.provider import AIContext, AIProviderError


def run(coro):
    return asyncio.run(coro)


def test_converse_returns_reply_and_records_history():
    engine = PersonalityEngine()
    seen_contexts: list[AIContext] = []

    async def fake_ask(message: str, context: AIContext) -> str:
        seen_contexts.append(context)
        return f"echo: {message}"

    async def body():
        reply = await engine.converse(fake_ask, "salut")
        assert reply == "echo: salut"
        assert seen_contexts[0].history == []  # first turn: no prior history yet
        assert seen_contexts[0].system  # persona always present

        await engine.converse(fake_ask, "ca va ?")
        # Second call sees the first exchange as history.
        assert seen_contexts[1].history == [("user", "salut"), ("assistant", "echo: salut")]

    run(body())


def test_history_is_bounded():
    engine = PersonalityEngine()

    async def fake_ask(message: str, context: AIContext) -> str:
        return "ok"

    async def body():
        for i in range(MAX_HISTORY_TURNS + 5):
            await engine.converse(fake_ask, f"msg{i}")
        context = engine.build_context()
        assert len(context.history) == MAX_HISTORY_TURNS * 2
        # Oldest turns dropped first -- the most recent exchange survives.
        assert context.history[-2] == ("user", f"msg{MAX_HISTORY_TURNS + 4}")

    run(body())


def test_reset_clears_history():
    engine = PersonalityEngine()

    async def fake_ask(message: str, context: AIContext) -> str:
        return "ok"

    async def body():
        await engine.converse(fake_ask, "hello")
        engine.reset()
        assert engine.build_context().history == []

    run(body())


def test_converse_emits_curious_then_happy_on_success():
    emotions: list[str] = []
    engine = PersonalityEngine(emotion_sink=emotions.append)

    async def fake_ask(message: str, context: AIContext) -> str:
        return "ok"

    run(engine.converse(fake_ask, "hi"))

    assert emotions == ["curious", "happy"]


def test_converse_emits_confused_and_reraises_on_provider_error():
    emotions: list[str] = []
    engine = PersonalityEngine(emotion_sink=emotions.append)

    async def failing_ask(message: str, context: AIContext) -> str:
        raise AIProviderError("boom")

    async def body():
        try:
            await engine.converse(failing_ask, "hi")
        except AIProviderError:
            pass
        else:
            raise AssertionError("expected AIProviderError")

    run(body())

    assert emotions == ["curious", "confused"]
    # A failed turn is never remembered as a real exchange.
    assert engine.build_context().history == []


def test_unknown_emotion_is_ignored_not_raised():
    calls: list[str] = []

    def sink(emotion: str) -> None:
        calls.append(emotion)

    engine = PersonalityEngine(emotion_sink=sink)
    engine._emote("not-a-real-emotion")

    assert calls == []


def test_emotion_sink_failure_does_not_break_conversation():
    def broken_sink(emotion: str) -> None:
        raise RuntimeError("serial link down")

    engine = PersonalityEngine(emotion_sink=broken_sink)

    async def fake_ask(message: str, context: AIContext) -> str:
        return "ok"

    reply = run(engine.converse(fake_ask, "hi"))

    assert reply == "ok"
