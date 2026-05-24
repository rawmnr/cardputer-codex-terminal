from __future__ import annotations

import asyncio
import json
from pathlib import Path
from types import SimpleNamespace
from typing import Any, AsyncIterator
import unittest

from cardputer_codex_terminal.config import AppConfig
from cardputer_codex_terminal.core import MiddlewareApp
from cardputer_codex_terminal.codex_transport import MockCodexTransport
from cardputer_codex_terminal.events import Event, EventType
from cardputer_codex_terminal.messages import CardputerMessage
from cardputer_codex_terminal.server import CardputerBridgeServer


CONTRACT_ROOT = Path(__file__).resolve().parents[2] / "tests" / "contracts"


def load_contract_cases() -> list[dict[str, Any]]:
    with (CONTRACT_ROOT / "serial_protocol_cases.json").open("r", encoding="utf-8") as handle:
        return json.load(handle)


class FakeSerialBridge:
    def __init__(self, server: CardputerBridgeServer) -> None:
        self.server = server
        self.sent_frames: list[str] = []
        self.bridge_connected_sent = False

    async def send(self, envelope: dict[str, Any]) -> list[str]:
        raw = json.dumps(envelope, ensure_ascii=False)
        self.sent_frames.append(raw)
        responses, self.bridge_connected_sent = await self.server.handle_raw_message(
            raw,
            bridge_connected_sent=self.bridge_connected_sent,
        )
        return responses

class FakeCodexTransport(MockCodexTransport):
    def __init__(self) -> None:
        self.prompts: list[tuple[str, str | None, str | None]] = []

    async def start_turn(self, prompt: str, thread_id: str | None = None, cwd: str | None = None) -> AsyncIterator[Any]:
        self.prompts.append((prompt, thread_id, cwd))
        async for reply in super().start_turn(prompt, thread_id=thread_id, cwd=cwd):
            yield reply


class ContractTests(unittest.TestCase):
    def test_shared_status_request_fixture_round_trips_through_bridge(self) -> None:
        case = next(item for item in load_contract_cases() if item["name"] == "status_request_snapshot")

        class FakeApp:
            def __init__(self) -> None:
                self.session = SimpleNamespace(state_epoch=0, workspace_path="C:/repo", branch="feature/cardputer", thread_id="thr_123")
                self.config = SimpleNamespace(bridge_token=None)

            async def handle_cardputer_message(self, _: CardputerMessage) -> list[Event]:
                return [
                    Event(
                        EventType.STATUS_SNAPSHOT,
                        {"workspace_path": "C:/repo", "branch": "feature/cardputer", "thread_id": "thr_123"},
                    )
                ]

        async def scenario() -> tuple[CardputerMessage, list[str]]:
            server = CardputerBridgeServer(FakeApp())
            serial = FakeSerialBridge(server)
            message = CardputerMessage.from_dict(case["envelope"])
            responses = await serial.send(message.to_dict())
            return message, responses

        received, responses = asyncio.run(scenario())
        self.assertEqual(received.to_dict(), case["envelope"])
        self.assertEqual(
            responses,
            [
                json.dumps({"type": "ack", "payload": {"id": case["envelope"]["id"], "ok": True}}, ensure_ascii=False),
                json.dumps(
                    {
                        "type": EventType.STATUS_SNAPSHOT.value,
                        "payload": {"workspace_path": "C:/repo", "branch": "feature/cardputer", "thread_id": "thr_123"},
                    },
                    ensure_ascii=False,
                ),
                json.dumps(
                    {
                        "type": "codex_status",
                        "payload": {
                            "kind": "bridge_connected",
                            "state_epoch": 0,
                            "workspace_path": "C:/repo",
                            "branch": "feature/cardputer",
                            "thread_id": "thr_123",
                        },
                    },
                    ensure_ascii=False,
                ),
            ],
        )

    def test_shared_prompt_fixture_uses_fake_codex_transport(self) -> None:
        case = next(item for item in load_contract_cases() if item["name"] == "text_prompt_with_auth")

        async def scenario() -> tuple[list[str], list[tuple[str, str | None, str | None]]]:
            app = MiddlewareApp(AppConfig(use_mock_codex=True))
            await app.initialize()
            transport = FakeCodexTransport()
            app.transport = transport
            server = CardputerBridgeServer(app)
            serial = FakeSerialBridge(server)
            responses = await serial.send(case["envelope"])
            return responses, transport.prompts

        responses, prompts = asyncio.run(scenario())
        self.assertEqual(prompts, [("hi", "mock-thread:.:main", ".")])
        self.assertTrue(responses[0].startswith('{"type": "ack"'))
        self.assertTrue(any(f'"type": "{EventType.CODEX_STATUS.value}"' in response for response in responses))
        self.assertTrue(any('"content": "mock_turn_completed"' in response for response in responses))
