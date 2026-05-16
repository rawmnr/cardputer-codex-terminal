from __future__ import annotations

import asyncio
import json
import unittest
from types import SimpleNamespace

from cardputer_codex_terminal.events import Event, EventType
from cardputer_codex_terminal.messages import CardputerMessage, CardputerMessageType
from cardputer_codex_terminal.server import CardputerBridgeServer


class BridgeServerTests(unittest.TestCase):
    def test_handle_raw_message_routes_and_serializes_events(self) -> None:
        class FakeApp:
            def __init__(self) -> None:
                self.session = SimpleNamespace(workspace_path="C:/repo", branch="feature/cardputer", thread_id="thr_123")
                self.config = SimpleNamespace(bridge_token=None)
                self.received: CardputerMessage | None = None

            async def handle_cardputer_message(self, message: CardputerMessage) -> list[Event]:
                self.received = message
                return [Event(EventType.CODEX_STATUS, {"kind": "bridge_status", "content": "ok"})]

        async def scenario() -> tuple[CardputerMessage | None, list[str]]:
            server = CardputerBridgeServer(FakeApp())
            payload = CardputerMessage(CardputerMessageType.STATUS_REQUEST, {}).to_dict()
            responses = await server.handle_raw_message(json.dumps(payload))
            return server.app.received, responses  # type: ignore[return-value]

        received, responses = asyncio.run(scenario())

        self.assertIsNotNone(received)
        self.assertEqual(received.type, CardputerMessageType.STATUS_REQUEST)
        self.assertEqual(
            responses,
            [
                json.dumps(
                    {
                        "type": EventType.CODEX_STATUS.value,
                        "payload": {"kind": "bridge_status", "content": "ok"},
                    },
                    ensure_ascii=False,
                )
            ],
        )

    def test_handle_raw_message_rejects_invalid_bridge_token(self) -> None:
        class FakeApp:
            def __init__(self) -> None:
                self.session = SimpleNamespace(workspace_path="C:/repo", branch="feature/cardputer", thread_id="thr_123")
                self.config = SimpleNamespace(bridge_token="secret")
                self.received: CardputerMessage | None = None

            async def handle_cardputer_message(self, message: CardputerMessage) -> list[Event]:
                self.received = message
                return [Event(EventType.CODEX_STATUS, {"kind": "bridge_status", "content": "ok"})]

        async def scenario() -> tuple[CardputerMessage | None, list[str]]:
            server = CardputerBridgeServer(FakeApp())
            payload = CardputerMessage(CardputerMessageType.STATUS_REQUEST, {}, auth_token="wrong").to_dict()
            responses = await server.handle_raw_message(json.dumps(payload))
            return server.app.received, responses  # type: ignore[return-value]

        received, responses = asyncio.run(scenario())

        self.assertIsNone(received)
        self.assertEqual(
            responses,
            [
                json.dumps(
                    {
                        "type": "error",
                        "payload": {
                            "kind": "bridge_auth_failed",
                            "content": "Bridge authentication failed.",
                        },
                    },
                    ensure_ascii=False,
                )
            ],
        )
