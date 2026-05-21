from __future__ import annotations

import asyncio
import unittest

from cardputer_codex_terminal.config import AppConfig
from cardputer_codex_terminal.core import MiddlewareApp
from cardputer_codex_terminal.events import Event, EventType
from cardputer_codex_terminal.messages import CardputerMessage, CardputerMessageType
from cardputer_codex_terminal.runs import AgentRun, ApprovalRequest, RunIndex, RunStatus


class RunModelTests(unittest.TestCase):
    def test_run_index_creates_and_finds_runs(self) -> None:
        index = RunIndex()

        run = index.create_run(
            workspace_path="C:/repo",
            base_branch="main",
            branch="feature/cardputer",
            thread_id="thr_123",
            session_id="session-000001",
        )

        self.assertEqual(index.current(), run)
        self.assertEqual(index.find_by_session_id("session-000001"), run)
        self.assertEqual(index.find_by_thread_id("thr_123"), run)
        self.assertEqual(index.to_dict()["active_run_id"], run.run_id)
        self.assertEqual(index.to_dict()["runs"][run.run_id]["session_id"], "session-000001")
        run.pending_approval = ApprovalRequest(approval_id="approval-2", title="Allow merge", detail="danger")
        self.assertEqual(index.find_by_approval_id("approval-2"), run)



    def test_agent_run_status_transitions_from_events(self) -> None:
        run = AgentRun("run-000001")

        run.record_event(Event(EventType.TEXT_PROMPT, {"content": "Build the firmware"}))
        self.assertEqual(run.status, RunStatus.RUNNING)
        self.assertEqual(run.current_step, "Build the firmware")
        self.assertEqual(run.last_event, "Build the firmware")

        run.record_event(
            Event(
                EventType.APPROVAL_REQUEST,
                {
                    "approval_id": "approval-1",
                    "title": "Allow write access",
                    "detail": "Need approval to continue.",
                    "options": ["Accept", "Reject"],
                    "selected_index": 1,
                },
            )
        )
        self.assertEqual(run.status, RunStatus.APPROVAL)
        self.assertIsNotNone(run.pending_approval)
        self.assertEqual(run.pending_approval.title, "Allow write access")
        self.assertEqual(run.pending_approval.options, ("Accept", "Reject"))

        run.record_event(Event(EventType.APPROVAL_RESPONSE, {"approved": True}))
        self.assertEqual(run.status, RunStatus.RUNNING)
        self.assertIsNone(run.pending_approval)

        run.record_event(Event(EventType.CODEX_STATUS, {"kind": "turn/completed", "content": "turn completed"}))
        self.assertEqual(run.status, RunStatus.DONE)
        self.assertEqual(run.last_event, "turn completed")

        run.record_event(
            Event(
                EventType.CODEX_STATUS,
                {
                    "kind": "run_marked_for_merge",
                    "content": "Marked run for merge",
                    "diff_summary": {"files_changed": 2, "insertions": 10, "deletions": 5, "summary": "feat: X"},
                    "test_summary": {"tests_run": 5, "passed": 4, "failed": 1, "skipped": 0, "summary": "5 tests, 1 fail"},
                },
            )
        )
        self.assertTrue(run.merge_ready)
        self.assertIsNotNone(run.diff_summary)
        self.assertEqual(run.diff_summary.files_changed, 2)
        self.assertIsNotNone(run.test_summary)
        self.assertEqual(run.test_summary.failed, 1)
        run.record_event(Event(EventType.ERROR, {"content": "boom"}))
        self.assertEqual(run.status, RunStatus.FAILED)
        self.assertEqual(run.last_event, "boom")



class RunIntegrationTests(unittest.TestCase):
    def test_status_request_includes_run_snapshot(self) -> None:
        async def scenario() -> dict[str, object]:
            app = MiddlewareApp(
                AppConfig(
                    host="127.0.0.1",
                    port=8765,
                    codex_ws_url="ws://127.0.0.1:9000",
                    use_mock_codex=True,
                )
            )
            await app.initialize()
            events = await app.handle_cardputer_message(CardputerMessage(CardputerMessageType.STATUS_REQUEST, {}))
            return events[0].to_dict()["payload"]

        payload = asyncio.run(scenario())

        self.assertEqual(payload["active_run_id"], "run-000001")
        self.assertEqual(len(payload["runs"]), 1)
        self.assertEqual(payload["runs"][0]["run_id"], payload["active_run_id"])
        self.assertEqual(payload["runs"][0]["session_id"], "session-000001")
        self.assertEqual(payload["sessions"][0]["run_id"], payload["active_run_id"])
        self.assertEqual(payload["runs"][0]["status"], "idle")
