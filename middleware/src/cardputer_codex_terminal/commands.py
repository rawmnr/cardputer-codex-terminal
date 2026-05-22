from __future__ import annotations

import asyncio
import contextlib
from pathlib import Path
from typing import TYPE_CHECKING, Any

from .events import Event, EventType
from .runs import AgentRun, RunMode, RunRole, RunStatus
from .worktree import WorktreeError

if TYPE_CHECKING:
    from .core import MiddlewareApp


class OrchestrationCommandRouter:
    def __init__(self, app: MiddlewareApp) -> None:
        self.app = app

    def _error(self, content: str, *, run_id: str | None = None) -> list[Event]:
        payload: dict[str, Any] = {"content": content}
        if run_id is not None:
            payload["run_id"] = run_id
        event = Event(EventType.ERROR, payload)
        record_in_run_index = run_id is not None and run_id in self.app.run_index.runs
        self.app._notify([event], run_id=run_id, record_in_run_index=record_in_run_index)
        return [event]

    def _run_or_error(self, run_id: str) -> AgentRun | list[Event]:
        run = self.app.run_index.runs.get(run_id)
        if run is None:
            return self._error(f"Run {run_id} not found")
        return run

    def _worktree_or_error(self, name: str) -> Path | list[Event]:
        try:
            worktree_path = self.app.worktree_manager.get_worktree_by_name(name)
        except WorktreeError as exc:
            return self._error(str(exc))
        if worktree_path is None:
            return self._error(f"Worktree {name} not found")
        return worktree_path

    async def handle_text(self, text: str) -> list[Event]:
        parts = text.strip().split()
        if not parts:
            return self._error("Empty command")

        cmd = parts[0]
        if cmd == "/worktrees":
            return await self._list_worktrees()
        if cmd == "/worktree":
            return await self._handle_worktree(parts)
        if cmd == "/run":
            return await self._handle_run(parts)
        if cmd == "/runs" and len(parts) == 2 and parts[1] == "cleanup":
            report = self.app.persistence.cleanup_orphans(self.app.run_index)
            self.app.persistence.save(self.app.run_index)
            content = f"Cleanup removed {report.removed_runs} orphan run(s)."
            event = Event(EventType.CODEX_STATUS, {"kind": "runs_cleanup", "content": content, "removed": report.removed_runs})
            self.app._notify([event], record_in_run_index=False)
            return [event]
        if cmd == "/mode":
            return await self._handle_mode(parts)
        return self._error(f"Unknown command: {text}")

    async def _list_worktrees(self) -> list[Event]:
        worktrees = await asyncio.to_thread(self.app.worktree_manager.list_worktrees)
        lines = [f"{wt.path.name} ({self.app.worktree_manager.normalize_branch_name(wt.branch)})" for wt in worktrees]
        content = "Worktrees:\n" + "\n".join(lines) if lines else "No worktrees found."
        event = Event(EventType.CODEX_STATUS, {"kind": "worktree_list", "content": content})
        self.app._notify([event], record_in_run_index=False)
        return [event]

    async def _handle_worktree(self, parts: list[str]) -> list[Event]:
        if len(parts) == 5 and parts[1] == "create" and parts[3] == "from":
            name, base_branch = parts[2], parts[4]
            try:
                path = await asyncio.to_thread(self.app.worktree_manager.create_worktree, base_branch, name)
            except WorktreeError as exc:
                return self._error(str(exc))
            content = f"Created worktree {name} from {base_branch} at {path}"
            event = Event(EventType.CODEX_STATUS, {"kind": "worktree_created", "content": content, "worktree_path": str(path), "branch": base_branch, "name": name})
            self.app._notify([event], record_in_run_index=False)
            return [event]

        if len(parts) == 3 and parts[1] == "remove":
            name = parts[2]
            worktree_path = await asyncio.to_thread(self.app.worktree_manager.get_worktree_by_name, name)
            if not worktree_path:
                return self._error(f"Worktree {name} not found")
            try:
                await asyncio.to_thread(self.app.worktree_manager.remove_worktree, worktree_path)
            except WorktreeError as exc:
                return self._error(str(exc))
            event = Event(EventType.CODEX_STATUS, {"kind": "worktree_removed", "content": f"Removed worktree {name}", "worktree_path": str(worktree_path)})
            self.app._notify([event], record_in_run_index=False)
            return [event]

        return self._error(f"Unknown worktree command: {' '.join(parts)}")

    async def _handle_run(self, parts: list[str]) -> list[Event]:
        if len(parts) >= 3 and parts[1] in {"approve", "reject"}:
            return await self._approve_or_reject(parts)
        if len(parts) >= 3 and parts[1] in {"stop", "pause", "pause_run", "resume", "resume_run", "merge", "mark-for-merge", "mark_for_merge", "mark_ready_for_merge", "diff", "collect_diff", "tests", "run_tests", "report", "generate_merge_report"}:
            return await self._run_action(parts)
        if len(parts) == 4 and parts[1] == "worker":
            return await self.start_worker(parts[2], parts[3])
        if len(parts) >= 3 and parts[1] == "start_worker":
            name = parts[2]
            mode = parts[3] if len(parts) > 3 else "safe"
            return await self.start_worker(name, mode)
        return self._error(f"Unknown run command: {' '.join(parts)}")

    async def _handle_mode(self, parts: list[str]) -> list[Event]:
        if len(parts) != 2:
            return self._error(f"Unknown mode command: {' '.join(parts)}")
        mode_str = parts[1].lower()
        run = self.app.run_index.current()
        if run is None:
            return self._error("No active run")
        if mode_str == "yolo" and self.app.worktree_manager.is_protected(run.branch or run.base_branch):
            return self._error(f"Cannot enable YOLO on protected branch {run.branch or run.base_branch}")
        new_mode = RunMode.SAFE if mode_str == "safe" else RunMode.YOLO_WORKTREE if mode_str == "yolo" else RunMode.REVIEW_ONLY
        run.mode = new_mode
        self.app._update_run_ui(run)
        event = Event(EventType.CODEX_STATUS, {"kind": "mode_changed", "content": f"Mode changed to {new_mode.value}", "mode": new_mode.value, "run_id": run.run_id})
        self.app._notify([event], run_id=run.run_id)
        return [event]

    async def _approve_or_reject(self, parts: list[str]) -> list[Event]:
        if len(parts) < 3:
            return self._error(f"Incomplete approval command: {' '.join(parts)}")
        action = parts[1]
        run_id = parts[2]
        run_or_error = self._run_or_error(run_id)
        if isinstance(run_or_error, list):
            return run_or_error
        approval_id = parts[3] if len(parts) > 3 else (run_or_error.pending_approval.approval_id if run_or_error.pending_approval else "")
        note = "Approve once" if action == "approve" else "Rejected on Cardputer"
        event = await self.app.handle_approval_response(action == "approve", approval_id=approval_id or None, note=note)
        return [event]

    async def _run_action(self, parts: list[str]) -> list[Event]:
        action = parts[1]
        run_id = parts[2]
        run_or_error = self._run_or_error(run_id)
        if isinstance(run_or_error, list):
            return run_or_error
        run = run_or_error

        if action in {"stop", "pause", "pause_run", "resume", "resume_run"}:
            return await self._transition_run(run, action)
        if action in {"merge", "mark-for-merge", "mark_for_merge", "mark_ready_for_merge"}:
            return await self.mark_ready_for_merge(run)
        if action in {"diff", "collect_diff"}:
            return await self.collect_diff(run)
        if action in {"tests", "run_tests"}:
            return await self.run_tests(run)
        if action in {"report", "generate_merge_report"}:
            return await self.generate_merge_report(run)
        return self._error(f"Unknown run action: {action}", run_id=run_id)

    async def start_worker(self, name: str, mode_text: str) -> list[Event]:
        worktree_path_or_error = self._worktree_or_error(name)
        if isinstance(worktree_path_or_error, list):
            return worktree_path_or_error
        worktree_path = worktree_path_or_error

        info = await asyncio.to_thread(self.app.worktree_manager.get_worktree_info_by_name, name)
        branch = info.branch if info is not None else self.app.config.branch or "main"
        normalized_branch = self.app.worktree_manager.normalize_branch_name(branch)
        base_branch = self.app.config.branch or self.app.session.branch or normalized_branch
        mode = self._parse_mode(mode_text)
        if mode == RunMode.YOLO_WORKTREE and self.app.worktree_manager.is_protected(base_branch):
            return self._error(f"Cannot run YOLO on protected branch {base_branch}")

        run = self.app.run_index.create_run(
            workspace_path=str(worktree_path),
            base_branch=base_branch,
            worktree_path=str(worktree_path),
            branch=normalized_branch,
            session_id=self.app.session.session_id,
            role=RunRole.WORKER,
            mode=mode,
            status=RunStatus.IDLE,
            current_step=f"{name} ({mode.value})",
            last_event=f"Worker queued in {name}",
        )
        try:
            handle = await self.app.supervisor.start_run(run)
        except Exception as exc:
            run.status = RunStatus.FAILED
            event = Event(EventType.ERROR, {"content": f"Failed to start run {run.run_id}: {exc}", "run_id": run.run_id})
            self.app._notify([event], run_id=run.run_id)
            return [event]

        event = Event(
            EventType.CODEX_STATUS,
            {
                "kind": "run_started",
                "content": f"Started run {run.run_id} in {name} ({mode.value})",
                "run_id": run.run_id,
                "thread_id": handle.thread_id,
                "worktree_path": str(worktree_path),
                "branch": normalized_branch,
                "mode": mode.value,
                "dedicated": handle.dedicated,
            },
        )
        self.app._update_run_ui(run)
        self.app._notify([event], run_id=run.run_id)
        return [event]

    async def pause_run(self, run: AgentRun) -> list[Event]:
        handle = self.app.supervisor.get_handle(run.run_id)
        if run.thread_id:
            await self.app.supervisor.interrupt(run.run_id)
            if handle is None:
                with contextlib.suppress(Exception):
                    await self.app.transport.interrupt_turn(run.thread_id)
        run.status = RunStatus.PAUSED
        event = Event(EventType.CODEX_STATUS, {"kind": "run_paused", "content": f"Paused run {run.run_id}", "run_id": run.run_id})
        self.app._update_run_ui(run)
        self.app._notify([event], run_id=run.run_id)
        return [event]

    async def resume_run(self, run: AgentRun) -> list[Event]:
        if self.app.supervisor.get_handle(run.run_id) is None:
            run.thread_id = None
        try:
            handle = await self.app.supervisor.start_run(run)
        except Exception as exc:
            run.status = RunStatus.FAILED
            event = Event(EventType.ERROR, {"content": f"Failed to resume run {run.run_id}: {exc}", "run_id": run.run_id})
            self.app._notify([event], run_id=run.run_id)
            return [event]
        run.status = RunStatus.RUNNING
        event = Event(
            EventType.CODEX_STATUS,
            {"kind": "run_resumed", "content": f"Resumed run {run.run_id}", "run_id": run.run_id, "thread_id": handle.thread_id},
        )
        self.app._update_run_ui(run)
        self.app._notify([event], run_id=run.run_id)
        return [event]

    async def stop_run(self, run: AgentRun) -> list[Event]:
        handle = self.app.supervisor.get_handle(run.run_id)
        if handle is not None and handle.dedicated:
            await self.app.supervisor.kill(run.run_id)
        elif run.thread_id:
            await self.app.supervisor.interrupt(run.run_id)
            if handle is None:
                with contextlib.suppress(Exception):
                    await self.app.transport.interrupt_turn(run.thread_id)
        run.status = RunStatus.PAUSED
        event = Event(EventType.CODEX_STATUS, {"kind": "run_stopped", "content": f"Stopped run {run.run_id}", "run_id": run.run_id})
        self.app._update_run_ui(run)
        self.app._notify([event], run_id=run.run_id)
        return [event]

    async def collect_diff(self, run: AgentRun) -> list[Event]:
        path = run.worktree_path or run.workspace_path
        diff = await asyncio.to_thread(self.app.worktree_manager.collect_diff, Path(path), run.base_branch)
        run.diff_summary = diff
        content = diff.summary or f"{diff.files_changed} file(s), +{diff.insertions}/-{diff.deletions}"
        event = Event(EventType.CODEX_STATUS, {"kind": "diff_collected", "content": content, "run_id": run.run_id, "diff_summary": diff.to_dict()})
        self.app._update_run_ui(run)
        self.app._notify([event], run_id=run.run_id)
        return [event]

    async def run_tests(self, run: AgentRun) -> list[Event]:
        path = run.worktree_path or run.workspace_path
        test = await asyncio.to_thread(self.app.worktree_manager.run_tests, Path(path))
        run.test_summary = test
        content = test.summary or f"{test.tests_run} test(s), {test.failed} failed"
        event = Event(EventType.CODEX_STATUS, {"kind": "tests_collected", "content": content, "run_id": run.run_id, "test_summary": test.to_dict()})
        self.app._update_run_ui(run)
        self.app._notify([event], run_id=run.run_id)
        return [event]

    async def mark_ready_for_merge(self, run: AgentRun) -> list[Event]:
        if run.status not in {RunStatus.DONE, RunStatus.FAILED}:
            return self._error(f"Run {run.run_id} is not ready for merge", run_id=run.run_id)
        run.merge_ready = True
        event = Event(EventType.CODEX_STATUS, {"kind": "run_marked_for_merge", "content": f"Marked run {run.run_id} for merge", "run_id": run.run_id})
        self.app._update_run_ui(run)
        self.app._notify([event], run_id=run.run_id)
        return [event]

    async def generate_merge_report(self, run: AgentRun) -> list[Event]:
        report = await asyncio.to_thread(self.app.worktree_manager.generate_merge_report, run)
        event = Event(EventType.CODEX_STATUS, {"kind": "merge_report_generated", "content": "Merge report ready", "run_id": run.run_id, "report": report})
        self.app._notify([event], run_id=run.run_id)
        return [event]

    async def _transition_run(self, run: AgentRun, action: str) -> list[Event]:
        if action in {"pause", "pause_run"}:
            return await self.pause_run(run)
        if action in {"resume", "resume_run"}:
            return await self.resume_run(run)
        return await self.stop_run(run)

    def _parse_mode(self, mode_text: str) -> RunMode:
        mode = mode_text.lower()
        if mode in {"yolo", "yolo_worktree", "yolo-wt"}:
            return RunMode.YOLO_WORKTREE
        if mode in {"review", "review_only", "review-only"}:
            return RunMode.REVIEW_ONLY
        return RunMode.SAFE
