from __future__ import annotations

import asyncio
import unittest

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
        transport = LocalWebSocketCodexTransport("ws://127.0.0.1:9000")

        with self.assertRaises(NotImplementedError) as ctx:
            asyncio.run(transport.initialize())

        self.assertIn("ws://127.0.0.1:9000", str(ctx.exception))

