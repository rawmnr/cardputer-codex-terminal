from __future__ import annotations

import asyncio
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from cardputer_codex_terminal.config import AppConfig
from cardputer_codex_terminal.core import MiddlewareApp
from cardputer_codex_terminal.events import EventType
from cardputer_codex_terminal.runs import AgentRun, DiffSummary, RunMode, RunRole, RunStatus, TestSummary as RunTestSummary
from cardputer_codex_terminal.supervisor import CodexHandle

from cardputer_codex_terminal.worktree import WorktreeInfo


class FakeTransport:
    def __init__(self, label: str) -> None:
        self.label = label
        self.initialized = 0
        self.started: list[tuple[str, str | None]] = []
        self.resumed: list[str] = []
        self.updated: list[tuple[str, str | None]] = []
        self.interrupted: list[str] = []

    async def initialize(self) -> None:
        self.initialized += 1

    async def start_thread(self, cwd: str, branch: str | None = None) -> str:
        self.started.append((cwd, branch))
        return f"{self.label}:{cwd}:{len(self.started)}"

    async def resume_thread(self, thread_id: str) -> str:
        self.resumed.append(thread_id)
        return thread_id

    async def update_thread_metadata(self, thread_id: str, branch: str | None = None) -> None:
        self.updated.append((thread_id, branch))

    async def submit_approval(self, approval_id: str, approved: bool, note: str | None = None) -> None:
        return None

    async def interrupt_turn(self, thread_id: str) -> None:
        self.interrupted.append(thread_id)

    async def start_turn(self, prompt: str, thread_id: str | None = None, cwd: str | None = None):
        raise AssertionError("start_turn is not used in command tests")


class CommandTests(unittest.TestCase):
    def _make_app(self) -> tuple[MiddlewareApp, tempfile.TemporaryDirectory[str]]:
        tempdir = tempfile.TemporaryDirectory()
        app = MiddlewareApp(
            AppConfig(
                host="127.0.0.1",
                port=8765,
                codex_ws_url="ws://127.0.0.1:9000",
                use_mock_codex=True,
                workspace_path=tempdir.name,
                state_path=str(Path(tempdir.name) / "state.json"),
            )
        )
        return app, tempdir

    def test_create_worktree_returns_compact_event(self) -> None:
        async def scenario() -> tuple[list[dict[str, object]], RunStatus]:
            app, tempdir = self._make_app()
            try:
                await app.initialize()
                with patch.object(app.worktree_manager, "create_worktree", return_value=Path("/tmp/worktrees/demo")) as create_worktree:
                    events = await app.command_router.handle_text("/worktree create demo from main")
                create_worktree.assert_called_once_with("main", "demo")
                return [event.to_dict() for event in events], app.run_index.current().status
            finally:
                await app.close()
                tempdir.cleanup()

        payloads, status = asyncio.run(scenario())

        self.assertEqual(payloads[0]["payload"]["kind"], "worktree_created")
        self.assertIn("Created worktree demo from main", payloads[0]["payload"]["content"])
        self.assertEqual(status.value, "idle")

    def test_start_worker_blocks_yolo_on_protected_branch(self) -> None:
        async def scenario() -> list[dict[str, object]]:
            app, tempdir = self._make_app()
            try:
                await app.initialize()
                with (
                    patch.object(app.worktree_manager, "get_worktree_by_name", return_value=Path("/repo/worktrees/yolo")),
                    patch.object(app.worktree_manager, "get_worktree_info_by_name", return_value=WorktreeInfo(Path("/repo/worktrees/yolo"), "deadbeef", "main")),
                    patch.object(app.worktree_manager, "is_protected", return_value=True),
                ):
                    events = await app.command_router.start_worker("yolo", "yolo")
                return [event.to_dict() for event in events]
            finally:
                await app.close()
                tempdir.cleanup()

        payloads = asyncio.run(scenario())

        self.assertEqual(payloads[0]["type"], EventType.ERROR.value)
        self.assertIn("Cannot run YOLO on protected branch", payloads[0]["payload"]["content"])

    def test_start_worker_uses_dedicated_transport_for_yolo(self) -> None:
        async def scenario() -> tuple[list[dict[str, object]], AgentRun, FakeTransport]:
            app, tempdir = self._make_app()
            dedicated: list[FakeTransport] = []
            try:
                await app.initialize()
                app.supervisor.transport_factory = lambda command, cwd: dedicated.append(FakeTransport(cwd)) or dedicated[-1]
                with (
                    patch.object(app.worktree_manager, "get_worktree_by_name", return_value=Path("/repo/worktrees/yolo")),
                    patch.object(app.worktree_manager, "get_worktree_info_by_name", return_value=WorktreeInfo(Path("/repo/worktrees/yolo"), "deadbeef", "feature/cardputer")),
                    patch.object(app.worktree_manager, "is_protected", return_value=False),
                ):
                    events = await app.command_router.start_worker("yolo", "yolo")
                run = app.run_index.current()
                assert run is not None
                return [event.to_dict() for event in events], run, dedicated[0]
            finally:
                await app.close()
                tempdir.cleanup()

        payloads, run, transport = asyncio.run(scenario())

        self.assertEqual(payloads[0]["payload"]["kind"], "run_started")
        self.assertEqual(run.mode, RunMode.YOLO_WORKTREE)
        self.assertEqual(run.status, RunStatus.RUNNING)
        self.assertEqual(transport.started[0][1], "feature/cardputer")
        self.assertEqual(Path(transport.started[0][0]), Path("/repo/worktrees/yolo"))
        self.assertEqual(transport.updated, [(run.thread_id, "feature/cardputer")])
        self.assertTrue(payloads[0]["payload"]["dedicated"])

    def test_run_actions_update_only_target_run(self) -> None:
        async def scenario() -> tuple[dict[str, object], dict[str, object], dict[str, object], dict[str, object], AgentRun, AgentRun]:
            app, tempdir = self._make_app()
            try:
                await app.initialize()
                run_a = app.run_index.create_run(
                    workspace_path=str(tempdir.name),
                    worktree_path=str(Path(tempdir.name) / "wt-a"),
                    branch="feature/a",
                    role=RunRole.WORKER,
                    mode=RunMode.SAFE,
                    status=RunStatus.RUNNING,
                )
                run_b = app.run_index.create_run(
                    workspace_path=str(tempdir.name),
                    worktree_path=str(Path(tempdir.name) / "wt-b"),
                    branch="feature/b",
                    role=RunRole.WORKER,
                    mode=RunMode.SAFE,
                    status=RunStatus.RUNNING,
                )
                run_a.thread_id = "thr-a"
                run_b.thread_id = "thr-b"
                run_a.status = RunStatus.RUNNING
                run_b.status = RunStatus.RUNNING

                pause_events = await app.command_router.pause_run(run_a)
                resume_events = await app.command_router.resume_run(run_a)
                stop_events = await app.command_router.stop_run(run_a)
                run_a.status = RunStatus.DONE
                merge_events = await app.command_router.mark_ready_for_merge(run_a)
                return (
                    pause_events[0].to_dict(),
                    resume_events[0].to_dict(),
                    stop_events[0].to_dict(),
                    merge_events[0].to_dict(),
                    run_a,
                    run_b,
                )
            finally:
                await app.close()
                tempdir.cleanup()

        pause_event, resume_event, stop_event, merge_event, run_a, run_b = asyncio.run(scenario())

        self.assertEqual(pause_event["payload"]["kind"], "run_paused")
        self.assertEqual(resume_event["payload"]["kind"], "run_resumed")
        self.assertEqual(stop_event["payload"]["kind"], "run_stopped")
        self.assertEqual(merge_event["payload"]["kind"], "run_marked_for_merge")
        self.assertEqual(run_a.status, RunStatus.DONE)
        self.assertTrue(run_a.merge_ready)
        self.assertEqual(run_b.status, RunStatus.RUNNING)
        self.assertFalse(run_b.merge_ready)

    def test_worktree_summaries_and_merge_report_are_projected(self) -> None:
        async def scenario() -> tuple[dict[str, object], dict[str, object], dict[str, object], AgentRun]:
            app, tempdir = self._make_app()
            try:
                await app.initialize()
                run = app.run_index.create_run(
                    workspace_path=str(tempdir.name),
                    worktree_path=str(Path(tempdir.name) / "wt-a"),
                    branch="feature/a",
                    role=RunRole.WORKER,
                    mode=RunMode.SAFE,
                    status=RunStatus.DONE,
                )
                with (
                    patch.object(app.worktree_manager, "collect_diff", return_value=DiffSummary(files_changed=2, insertions=10, deletions=3, summary="feat: something")),
                    patch.object(app.worktree_manager, "run_tests", return_value=RunTestSummary(tests_run=5, passed=4, failed=1, summary="5 tests, 1 failed")),
                    patch.object(app.worktree_manager, "generate_merge_report", return_value="merge report text"),
                ):
                    diff_event = (await app.command_router.collect_diff(run))[0].to_dict()
                    test_event = (await app.command_router.run_tests(run))[0].to_dict()
                    report_event = (await app.command_router.generate_merge_report(run))[0].to_dict()
                return diff_event, test_event, report_event, run
            finally:
                await app.close()
                tempdir.cleanup()

        diff_event, test_event, report_event, run = asyncio.run(scenario())

        self.assertEqual(diff_event["payload"]["kind"], "diff_collected")
        self.assertEqual(diff_event["payload"]["diff_summary"]["files_changed"], 2)
        self.assertEqual(test_event["payload"]["kind"], "tests_collected")
        self.assertEqual(test_event["payload"]["test_summary"]["failed"], 1)
        self.assertEqual(report_event["payload"]["kind"], "merge_report_generated")
        self.assertEqual(report_event["payload"]["report"], "merge report text")
        self.assertIsNotNone(run.diff_summary)
        self.assertIsNotNone(run.test_summary)
        self.assertEqual(run.diff_summary.files_changed, 2)
        self.assertEqual(run.test_summary.failed, 1)
