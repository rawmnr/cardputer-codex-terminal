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
from cardputer_codex_terminal.messages import CardputerMessage, CardputerMessageType, PROTOCOL_VERSION
from cardputer_codex_terminal.projections import CardputerProjection
from cardputer_codex_terminal.runs import ApprovalRequest, RunIndex, RunMode, RunRole, RunStatus
from cardputer_codex_terminal.server import CardputerBridgeServer
from cardputer_codex_terminal.session import SessionIndex, SessionState


CONTRACT_ROOT = Path(__file__).resolve().parents[2] / "tests" / "contracts"
PROJECTION_FIXTURES = CONTRACT_ROOT / "projection_fixtures.json"


def load_contract_cases() -> list[dict[str, Any]]:
    with (CONTRACT_ROOT / "serial_protocol_cases.json").open("r", encoding="utf-8") as handle:
        return json.load(handle)


def load_projection_fixtures() -> list[dict[str, Any]]:
    with PROJECTION_FIXTURES.open("r", encoding="utf-8") as handle:
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

    def test_protocol_version_matches_contract_cases_and_default_serialization(self) -> None:
        for case in load_contract_cases():
            self.assertEqual(case["envelope"]["protocol_version"], PROTOCOL_VERSION)

        envelope = CardputerMessage(CardputerMessageType.PING, {}).to_dict()
        self.assertEqual(envelope["protocol_version"], PROTOCOL_VERSION)

    def test_projections_match_contract_fixtures(self) -> None:
        fixtures = load_projection_fixtures()
        session_index = SessionIndex()
        run_index = RunIndex()
        projection = CardputerProjection(session_index, run_index)

        snapshot_case = next(f for f in fixtures if f["name"] == "status_snapshot_v1")
        session = session_index.ensure_active(workspace_path="C:/repo", branch="feature/cardputer", thread_id="thr_123", title="My Session")
        session.last_event = "File written"
        session.codex_usage_percent = 42
        proj = projection.build_status_snapshot(session)
        self.assertEqual(proj["type"], snapshot_case["type"])
        self.assertEqual(proj["v"], snapshot_case["v"])
        self.assertEqual(proj["payload"], snapshot_case["payload"])

        session_status_case = next(f for f in fixtures if f["name"] == "session_status_v1")
        run = run_index.create_run(role=RunRole.WORKER, mode=RunMode.SAFE, status=RunStatus.RUNNING)
        run.current_step = "Implementing feature X"
        run.branch = "feature/x"
        run.last_event = "File written"
        run.thread_id = "thread-123"
        run.worktree_path = "/tmp/wt"
        run.pending_approval = ApprovalRequest(approval_id="app-7", title="Allow write?", detail="danger", danger=True)
        proj = projection.build_session_status(session)
        self.assertEqual(proj["type"], session_status_case["type"])
        self.assertEqual(proj["v"], session_status_case["v"])
        self.assertEqual(proj["payload"], session_status_case["payload"])

        run_list_case = next(f for f in fixtures if f["name"] == "run_list_v1")
        proj = projection.build_run_list()
        self.assertEqual(proj["type"], run_list_case["type"])
        self.assertEqual(proj["v"], run_list_case["v"])
        self.assertEqual(proj["payload"], run_list_case["payload"])

        run_detail_case = next(f for f in fixtures if f["name"] == "run_detail_v1")
        proj = projection.build_run_detail(run.run_id)
        self.assertEqual(proj["type"], run_detail_case["type"])
        self.assertEqual(proj["v"], run_detail_case["v"])
        self.assertEqual(proj["payload"], run_detail_case["payload"])

        approval_inbox_case = next(f for f in fixtures if f["name"] == "approval_inbox_v1")
        proj = projection.build_approval_inbox()
        self.assertEqual(proj["type"], approval_inbox_case["type"])
        self.assertEqual(proj["v"], approval_inbox_case["v"])
        self.assertEqual(proj["payload"], approval_inbox_case["payload"])
