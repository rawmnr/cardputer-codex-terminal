from __future__ import annotations

import asyncio
import unittest

from cardputer_codex_terminal.runs import AgentRun, RunMode
from cardputer_codex_terminal.supervisor import CodexHandle, CodexProcessSupervisor


class FakeTransport:
    def __init__(self, label: str) -> None:
        self.label = label
        self.initialized = 0
        self.started: list[tuple[str, str | None]] = []
        self.resumed: list[str] = []
        self.updated: list[tuple[str, str | None]] = []
        self.interrupted: list[str] = []
        self.closed = False

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
        raise AssertionError("start_turn is not used in supervisor tests")

    async def close(self) -> None:
        self.closed = True


class SupervisorTests(unittest.TestCase):
    def test_yolo_run_uses_dedicated_worktree_transport(self) -> None:
        async def scenario() -> tuple[CodexHandle, AgentRun, FakeTransport]:
            shared = FakeTransport("shared")
            dedicated: list[FakeTransport] = []

            def factory(_: tuple[str, ...], cwd: str) -> FakeTransport:
                transport = FakeTransport(cwd)
                dedicated.append(transport)
                return transport

            supervisor = CodexProcessSupervisor(shared, ("codex", "app-server"), "/repo", transport_factory=factory)
            run = AgentRun(
                "run-000001",
                workspace_path="/repo",
                worktree_path="/repo/worktrees/yolo",
                branch="feature/cardputer",
                mode=RunMode.YOLO_WORKTREE,
            )
            handle = await supervisor.start_run(run)
            assert dedicated
            return handle, run, dedicated[0]

        handle, run, transport = asyncio.run(scenario())

        self.assertTrue(handle.dedicated)
        self.assertEqual(handle.cwd, "/repo/worktrees/yolo")
        self.assertEqual(run.status.value, "running")
        self.assertEqual(transport.initialized, 1)
        self.assertEqual(transport.started, [("/repo/worktrees/yolo", "feature/cardputer")])
        self.assertEqual(transport.updated, [(run.thread_id, "feature/cardputer")])

    def test_interrupt_and_kill_only_target_run(self) -> None:
        async def scenario() -> tuple[FakeTransport, FakeTransport, CodexProcessSupervisor, str, str]:
            shared = FakeTransport("shared")
            dedicated: list[FakeTransport] = []

            def factory(_: tuple[str, ...], cwd: str) -> FakeTransport:
                transport = FakeTransport(cwd)
                dedicated.append(transport)
                return transport

            supervisor = CodexProcessSupervisor(shared, ("codex", "app-server"), "/repo", transport_factory=factory)
            safe_run = AgentRun("run-000001", workspace_path="/repo", branch="main", mode=RunMode.SAFE)
            yolo_run = AgentRun("run-000002", workspace_path="/repo", worktree_path="/repo/worktrees/yolo", branch="feature/cardputer", mode=RunMode.YOLO_WORKTREE)
            safe_handle = await supervisor.start_run(safe_run)
            yolo_handle = await supervisor.start_run(yolo_run)
            await supervisor.interrupt(safe_run.run_id)
            await supervisor.kill(yolo_run.run_id)
            assert safe_handle.thread_id is not None
            assert yolo_handle.thread_id is not None
            return shared, dedicated[0], supervisor, safe_handle.thread_id, yolo_handle.thread_id

        shared, dedicated, supervisor, safe_thread, yolo_thread = asyncio.run(scenario())

        self.assertEqual(shared.interrupted, [safe_thread])
        self.assertFalse(shared.closed)
        self.assertTrue(dedicated.closed)
        self.assertIsNone(supervisor.get_handle("run-000002"))
        self.assertIsNotNone(supervisor.get_handle("run-000001"))
        self.assertEqual(dedicated.interrupted, [])
        self.assertEqual(yolo_thread, f"{dedicated.label}:/repo/worktrees/yolo:1")

    def test_close_cleans_up_dedicated_handles(self) -> None:
        async def scenario() -> tuple[FakeTransport, list[FakeTransport], CodexProcessSupervisor]:
            shared = FakeTransport("shared")
            dedicated: list[FakeTransport] = []

            def factory(_: tuple[str, ...], cwd: str) -> FakeTransport:
                transport = FakeTransport(cwd)
                dedicated.append(transport)
                return transport

            supervisor = CodexProcessSupervisor(shared, ("codex", "app-server"), "/repo", transport_factory=factory)
            run_a = AgentRun("run-000001", workspace_path="/repo", worktree_path="/repo/worktrees/a", branch="feature/a", mode=RunMode.YOLO_WORKTREE)
            run_b = AgentRun("run-000002", workspace_path="/repo", worktree_path="/repo/worktrees/b", branch="feature/b", mode=RunMode.YOLO_WORKTREE)
            await supervisor.start_run(run_a)
            await supervisor.start_run(run_b)
            await supervisor.close()
            return shared, dedicated, supervisor

        shared, dedicated, supervisor = asyncio.run(scenario())

        self.assertFalse(shared.closed)
        self.assertEqual(len(dedicated), 2)
        self.assertTrue(all(transport.closed for transport in dedicated))
        self.assertIsNone(supervisor.get_handle("run-000001"))
        self.assertIsNone(supervisor.get_handle("run-000002"))
