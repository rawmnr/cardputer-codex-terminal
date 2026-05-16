from __future__ import annotations

import asyncio
import json
import unittest
from dataclasses import asdict

from cardputer_codex_terminal.codex_transport import LocalWebSocketCodexTransport


class FakeWebSocket:
    def __init__(self, messages: list[dict]) -> None:
        self.sent: list[str] = []
        self.messages = [json.dumps(message) for message in messages]

    async def send(self, message: str) -> None:
        self.sent.append(message)

    async def recv(self) -> str:
        return self.messages.pop(0)


class TransportTests(unittest.TestCase):
    def test_transport_sends_initialize_and_turn_requests(self) -> None:
        async def connect_factory(_: str) -> FakeWebSocket:
            return FakeWebSocket(
                [
                    {"kind": "status", "content": "initialized"},
                    {"thread": {"id": "thr_123"}},
                    {"thread": {"id": "thr_123"}},
                    {"kind": "delta", "content": "hello"},
                    {"kind": "completed", "content": "done"},
                ]
            )

        async def scenario() -> tuple[list[str], list[dict]]:
            transport = LocalWebSocketCodexTransport("ws://127.0.0.1:9000", connect_factory=connect_factory)
            await transport.initialize()
            thread_id = await transport.start_thread("C:/repo", branch="feature/cardputer")
            replies = []
            async for reply in transport.start_turn("Hello Codex"):
                replies.append(asdict(reply))
            websocket = transport._ws
            assert websocket is not None
            return websocket.sent, replies

        sent, replies = asyncio.run(scenario())

        self.assertEqual(len(sent), 4)
        self.assertIn('"method": "initialize"', sent[0])
        self.assertIn('"method": "thread/start"', sent[1])
        self.assertIn('"method": "thread/metadata/update"', sent[2])
        self.assertIn('"method": "turn/start"', sent[3])
        self.assertEqual(
            replies,
            [
                {"kind": "delta", "content": "hello"},
                {"kind": "completed", "content": "done"},
            ],
        )
