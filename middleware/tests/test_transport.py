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
                    {"id": "initialize-1", "result": {}},
                    {"id": "thread/start-2", "result": {"thread": {"id": "thr_123"}}},
                    {"id": "thread/metadata/update-3", "result": {}},
                    {"id": "turn-start-4", "result": {"turn": {"id": "turn_123", "items": [], "status": "inProgress"}}},
                    {"method": "item/agentMessage/delta", "params": {"delta": "hello"}},
                    {"method": "turn/completed", "params": {"threadId": "thr_123", "turn": {"id": "turn_123", "items": [], "status": "completed"}}},
                ]
            )

        async def scenario() -> tuple[list[str], list[dict]]:
            transport = LocalWebSocketCodexTransport("ws://127.0.0.1:9000", connect_factory=connect_factory)
            await transport.initialize()
            thread_id = await transport.start_thread("C:/repo", branch="feature/cardputer")
            replies = []
            async for reply in transport.start_turn("Hello Codex", thread_id=thread_id, cwd="C:/repo"):
                replies.append(asdict(reply))
            websocket = transport._ws
            assert websocket is not None
            return websocket.sent, replies

        sent, replies = asyncio.run(scenario())

        self.assertEqual(len(sent), 5)
        self.assertIn('"method": "initialize"', sent[0])
        self.assertIn('"method": "initialized"', sent[1])
        self.assertIn('"method": "thread/start"', sent[2])
        self.assertIn('"method": "thread/metadata/update"', sent[3])
        self.assertIn('"method": "turn/start"', sent[4])
        self.assertIn('"threadId": "thr_123"', sent[4])
        self.assertIn('"cwd": "C:/repo"', sent[4])
        self.assertIn('"input": [{"type": "text", "text": "Hello Codex"}]', sent[4])
        self.assertEqual(
            replies,
            [
                {"kind": "delta", "content": "hello", "data": {"delta": "hello"}},
                {
                    "kind": "completed",
                    "content": "turn_completed",
                    "data": {"threadId": "thr_123", "turn": {"id": "turn_123", "items": [], "status": "completed"}},
                },
            ],
        )

    def test_transport_submits_approval_response(self) -> None:
        async def connect_factory(_: str) -> FakeWebSocket:
            return FakeWebSocket(
                [
                    {"id": "initialize-1", "result": {}},
                ]
            )

        async def scenario() -> list[str]:
            transport = LocalWebSocketCodexTransport("ws://127.0.0.1:9000", connect_factory=connect_factory)
            await transport.initialize()
            await transport.submit_approval("appr_123", True, note="Approved on the Cardputer")
            websocket = transport._ws
            assert websocket is not None
            return websocket.sent

        sent = asyncio.run(scenario())

        self.assertEqual(len(sent), 3)
        self.assertIn('"method": "initialize"', sent[0])
        self.assertIn('"method": "initialized"', sent[1])
        self.assertIn('"id": "appr_123"', sent[2])
        self.assertIn('"decision": "accept"', sent[2])
        self.assertNotIn("Approved on the Cardputer", sent[2])

    def test_transport_formats_permissions_approval_response(self) -> None:
        async def connect_factory(_: str) -> FakeWebSocket:
            return FakeWebSocket(
                [
                    {"id": "initialize-1", "result": {}},
                    {"id": "turn-start-2", "result": {"turn": {"id": "turn_123", "items": [], "status": "inProgress"}}},
                    {
                        "id": "permissions-1",
                        "method": "item/permissions/requestApproval",
                        "params": {
                            "itemId": "item_123",
                            "threadId": "thr_123",
                            "turnId": "turn_123",
                            "permissions": {"network": {"enabled": True}},
                        },
                    },
                ]
            )

        async def scenario() -> list[str]:
            transport = LocalWebSocketCodexTransport("ws://127.0.0.1:9000", connect_factory=connect_factory)
            await transport.initialize()
            async for reply in transport.start_turn("Needs network", thread_id="thr_123"):
                self.assertEqual(reply.kind, "approval_request")
                break
            await transport.submit_approval("permissions-1", True)
            websocket = transport._ws
            assert websocket is not None
            return websocket.sent

        sent = asyncio.run(scenario())

        self.assertIn('"id": "permissions-1"', sent[-1])
        self.assertIn('"permissions": {"network": {"enabled": true}}', sent[-1])
        self.assertIn('"scope": "turn"', sent[-1])

    def test_stdio_transport_rejects_empty_command(self) -> None:
        from cardputer_codex_terminal.codex_transport import StdioCodexAppServerTransport

        transport = StdioCodexAppServerTransport(command=[])

        with self.assertRaises(ValueError):
            transport._resolve_command([])
