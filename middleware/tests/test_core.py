from __future__ import annotations

import asyncio
import json
import unittest
from dataclasses import asdict

from cardputer_codex_terminal.config import AppConfig
from cardputer_codex_terminal.core import MiddlewareApp
from cardputer_codex_terminal.codex_transport import LocalWebSocketCodexTransport
from cardputer_codex_terminal.events import EventType
from cardputer_codex_terminal.messages import CardputerMessage, CardputerMessageType


class MiddlewareAppTests(unittest.TestCase):
    def test_mock_transport_initialization_and_prompt_flow(self) -> None:
        async def scenario() -> list[dict]:
            app = MiddlewareApp(
                AppConfig(
                    host="127.0.0.1",
                    port=8765,
                    codex_ws_url="ws://127.0.0.1:9000",
                    use_mock_codex=True,
                )
            )
            await app.initialize()
            events = await app.handle_text_prompt("Hello Codex")
            return [event.to_dict() for event in events]

        payloads = asyncio.run(scenario())

        self.assertEqual(
            payloads,
            [
                {
                    "type": EventType.CODEX_STATUS.value,
                    "payload": {"content": "mock_codex_ready", "kind": "status", "data": {}},
                },
                {
                    "type": EventType.CODEX_DELTA.value,
                    "payload": {"content": "Received: Hello Codex", "kind": "delta", "data": {}},
                },
                {
                    "type": EventType.CODEX_USAGE.value,
                    "payload": {
                        "content": "mock_usage_update",
                        "kind": "usage",
                        "data": {"threadId": "mock-thread:.:main", "cwd": ".", "usedPercent": 12},
                    },
                },
                {
                    "type": EventType.CODEX_DELTA.value,
                    "payload": {"content": "This transport is a local placeholder.", "kind": "delta", "data": {}},
                },
                {
                    "type": EventType.CODEX_STATUS.value,
                    "payload": {"content": "mock_turn_completed", "kind": "completed", "data": {}},
                },
            ],
        )

    def test_project_and_branch_selection_update_session_and_thread(self) -> None:
        class FakeWebSocket:
            def __init__(self) -> None:
                self.sent: list[str] = []
                self.messages = [
                    json.dumps({"kind": "status", "content": "initialized"}),
                    json.dumps({"thread": {"id": "thr_123"}}),
                    json.dumps({"thread": {"id": "thr_123"}}),
                    json.dumps({"kind": "completed", "content": "done"}),
                ]

            async def send(self, message: str) -> None:
                self.sent.append(message)

            async def recv(self) -> str:
                return self.messages.pop(0)

        async def connect_factory(_: str) -> FakeWebSocket:
            return FakeWebSocket()

        async def scenario() -> tuple[list[str], dict[str, object]]:
            app = MiddlewareApp(
                AppConfig(
                    host="127.0.0.1",
                    port=8765,
                    codex_ws_url="ws://127.0.0.1:9000",
                    use_mock_codex=False,
                )
            )
            assert isinstance(app.transport, LocalWebSocketCodexTransport)
            app.transport = LocalWebSocketCodexTransport("ws://127.0.0.1:9000", connect_factory=connect_factory)

            await app.initialize()
            await app.handle_cardputer_message(
                CardputerMessage(CardputerMessageType.PROJECT_SELECT, {"workspace_path": "C:/repo"})
            )
            await app.handle_cardputer_message(
                CardputerMessage(CardputerMessageType.BRANCH_SELECT, {"branch": "feature/cardputer"})
            )
            events = await app.handle_cardputer_message(
                CardputerMessage(CardputerMessageType.TEXT_PROMPT, {"text": "Hello Codex"})
            )
            websocket = app.transport._ws
            assert websocket is not None
            return websocket.sent, {
                "session": {
                    "workspace_path": app.session.workspace_path,
                    "branch": app.session.branch,
                    "thread_id": app.session.thread_id,
                },
                "events": [event.to_dict() for event in events],
            }

        sent, state = asyncio.run(scenario())

        self.assertEqual(state["session"], {"workspace_path": "C:/repo", "branch": "feature/cardputer", "thread_id": "thr_123"})
        self.assertTrue(any('"method": "thread/start"' in item for item in sent))
        self.assertTrue(any('"method": "thread/metadata/update"' in item for item in sent))
        self.assertTrue(any('"method": "turn/start"' in item for item in sent))
        self.assertEqual(
            state["events"],
            [{"type": EventType.CODEX_STATUS.value, "payload": {"content": "done", "kind": "completed", "data": {}}}],
        )

    def test_local_websocket_transport_reports_target_url(self) -> None:
        class FakeWebSocket:
            def __init__(self) -> None:
                self.sent: list[str] = []
                self.messages = [
                    json.dumps({"kind": "status", "content": "initialized"}),
                    json.dumps({"kind": "delta", "content": "partial"}),
                    json.dumps({"kind": "completed", "content": "done"}),
                ]

            async def send(self, message: str) -> None:
                self.sent.append(message)

            async def recv(self) -> str:
                return self.messages.pop(0)

        async def connect_factory(_: str) -> FakeWebSocket:
            return FakeWebSocket()

        transport = LocalWebSocketCodexTransport("ws://127.0.0.1:9000", connect_factory=connect_factory)

        async def scenario() -> list[dict]:
            await transport.initialize()
            events = []
            async for reply in transport.start_turn("Hello Codex"):
                events.append(reply)
            return [asdict(event) for event in events]

        payloads = asyncio.run(scenario())

        self.assertEqual(
            payloads,
            [
                {"kind": "delta", "content": "partial", "data": {}},
                {"kind": "completed", "content": "done", "data": {}},
            ],
        )

    def test_local_websocket_transport_parses_usage_notifications(self) -> None:
        class FakeWebSocket:
            def __init__(self) -> None:
                self.sent: list[str] = []
                self.messages = [
                    json.dumps({"kind": "status", "content": "initialized"}),
                    json.dumps(
                        {
                            "kind": "thread/tokenUsage/updated",
                            "tokenUsage": {"inputTokens": 120, "outputTokens": 42, "cachedInputTokens": 8, "reasoningTokens": 16},
                        }
                    ),
                    json.dumps({"kind": "completed", "content": "done"}),
                ]

            async def send(self, message: str) -> None:
                self.sent.append(message)

            async def recv(self) -> str:
                return self.messages.pop(0)

        async def connect_factory(_: str) -> FakeWebSocket:
            return FakeWebSocket()

        transport = LocalWebSocketCodexTransport("ws://127.0.0.1:9000", connect_factory=connect_factory)

        async def scenario() -> list[dict]:
            await transport.initialize()
            replies = []
            async for reply in transport.start_turn("Hello Codex"):
                replies.append(asdict(reply))
            return replies

        replies = asyncio.run(scenario())

        self.assertEqual(
            replies,
            [
                {
                    "kind": "usage",
                    "content": "thread usage input=120 output=42 cached=8 reasoning=16",
                    "data": {
                        "tokenUsage": {
                            "inputTokens": 120,
                            "outputTokens": 42,
                            "cachedInputTokens": 8,
                            "reasoningTokens": 16,
                        },
                        "kind": "thread/tokenUsage/updated",
                    },
                },
                {"kind": "completed", "content": "done", "data": {}},
            ],
        )
