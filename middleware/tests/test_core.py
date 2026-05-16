from __future__ import annotations

import asyncio
import json
import unittest
from dataclasses import asdict

from cardputer_codex_terminal.config import AppConfig
from cardputer_codex_terminal.core import MiddlewareApp
from cardputer_codex_terminal.codex_transport import LocalWebSocketCodexTransport
from cardputer_codex_terminal.events import EventType


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
                {"type": EventType.CODEX_STATUS.value, "payload": {"content": "mock_codex_ready", "kind": "status"}},
                {
                    "type": EventType.CODEX_DELTA.value,
                    "payload": {"content": "Received: Hello Codex", "kind": "delta"},
                },
                {
                    "type": EventType.CODEX_DELTA.value,
                    "payload": {"content": "This transport is a local placeholder.", "kind": "delta"},
                },
                {
                    "type": EventType.CODEX_STATUS.value,
                    "payload": {"content": "mock_turn_completed", "kind": "completed"},
                },
            ],
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
                {"kind": "delta", "content": "partial"},
                {"kind": "completed", "content": "done"},
            ],
        )
