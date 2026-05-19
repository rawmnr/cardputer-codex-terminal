from __future__ import annotations

from dataclasses import dataclass, field
from enum import StrEnum
from typing import Any


def _trim_text(value: str, limit: int = 64) -> str:
    text = value.strip()
    if len(text) <= limit:
        return text
    return text[: limit - 3] + "..."


def _event_payload(event: Any) -> tuple[str, dict[str, Any]]:
    if hasattr(event, "type") and hasattr(event, "payload"):
        return str(event.type), dict(event.payload)
    if isinstance(event, dict):
        return str(event.get("type", "")), dict(event.get("payload", {}))
    return "unknown", {}


class RunRole(StrEnum):
    MAIN = "main"
    WORKER = "worker"
    REVIEWER = "reviewer"
    TESTER = "tester"


class RunMode(StrEnum):
    SAFE = "safe"
    YOLO_WORKTREE = "yolo_worktree"
    REVIEW_ONLY = "review_only"


class RunStatus(StrEnum):
    IDLE = "idle"
    RUNNING = "running"
    APPROVAL = "approval"
    FAILED = "failed"
    DONE = "done"
    PAUSED = "paused"


@dataclass(slots=True)
class ApprovalRequest:
    approval_id: str
    title: str = ""
    detail: str = ""
    timeout_seconds: int | None = None
    channel: str | None = None
    urgency: str | None = None
    danger: bool | None = None
    options: tuple[str, ...] = ()
    selected_index: int = 0

    def to_dict(self) -> dict[str, Any]:
        return {
            "approval_id": self.approval_id,
            "title": self.title,
            "detail": self.detail,
            "timeout_seconds": self.timeout_seconds,
            "channel": self.channel,
            "urgency": self.urgency,
            "danger": self.danger,
            "options": list(self.options),
            "selected_index": self.selected_index,
        }


@dataclass(slots=True)
class DiffSummary:
    files_changed: int = 0
    insertions: int = 0
    deletions: int = 0
    summary: str = ""

    def to_dict(self) -> dict[str, Any]:
        return {
            "files_changed": self.files_changed,
            "insertions": self.insertions,
            "deletions": self.deletions,
            "summary": self.summary,
        }


@dataclass(slots=True)
class TestSummary:
    tests_run: int = 0
    passed: int = 0
    failed: int = 0
    skipped: int = 0
    summary: str = ""

    def to_dict(self) -> dict[str, Any]:
        return {
            "tests_run": self.tests_run,
            "passed": self.passed,
            "failed": self.failed,
            "skipped": self.skipped,
            "summary": self.summary,
        }


@dataclass(slots=True)
class AgentRun:
    run_id: str
    role: RunRole = RunRole.MAIN
    workspace_path: str = "."
    base_branch: str = "main"
    worktree_path: str | None = None
    branch: str | None = None
    thread_id: str | None = None
    session_id: str | None = None
    mode: RunMode = RunMode.SAFE
    status: RunStatus = RunStatus.IDLE
    current_step: str = ""
    last_event: str = ""
    pending_approval: ApprovalRequest | None = None
    diff_summary: DiffSummary | None = None
    test_summary: TestSummary | None = None
    danger_summary: str = ""
    badge_mode: str = "SAFE"

    state_epoch: int = 0

    def touch(
        self,
        *,
        workspace_path: str | None = None,
        base_branch: str | None = None,
        worktree_path: str | None = None,
        branch: str | None = None,
        thread_id: str | None = None,
        session_id: str | None = None,
        current_step: str | None = None,
        last_event: str | None = None,
        danger_summary: str | None = None,
        badge_mode: str | None = None,
    ) -> None:
        if workspace_path is not None and workspace_path:
            self.workspace_path = workspace_path
        if base_branch is not None and base_branch:
            self.base_branch = base_branch
        if current_step is not None:
            self.current_step = current_step
        if last_event is not None:
            self.last_event = last_event
        if danger_summary is not None:
            self.danger_summary = danger_summary
        if badge_mode is not None:
            self.badge_mode = badge_mode
        if worktree_path is not None:
            self.worktree_path = worktree_path or None
        if branch is not None:
            self.branch = branch or None
        if thread_id is not None:
            self.thread_id = thread_id or None
        if session_id is not None:
            self.session_id = session_id or None
        self.state_epoch += 1

    def record_event(self, event: Any) -> None:
        self.state_epoch += 1
        event_type, payload = _event_payload(event)
        event_dict = event.to_dict() if hasattr(event, "to_dict") else event if isinstance(event, dict) else {"type": event_type, "payload": payload}
        if not isinstance(event_dict, dict):
            return

        kind = str(payload.get("kind") or "")
        content = str(payload.get("content") or payload.get("text") or payload.get("message") or "")

        if event_type == "text_prompt":
            self.status = RunStatus.RUNNING
            if content:
                self.current_step = _trim_text(content, 48)
                self.last_event = content
        elif event_type == "audio_chunk":
            self.status = RunStatus.RUNNING
            self.last_event = f"audio chunk {payload.get('chunk_id', '')}".strip()
        elif event_type == "approval_request":
            self.status = RunStatus.APPROVAL
            approval_id = str(payload.get("approval_id") or payload.get("approvalId") or "")
            timeout_seconds = payload.get("timeout_seconds") or payload.get("timeoutSeconds")
            if isinstance(timeout_seconds, bool):
                timeout_seconds = None
            self.pending_approval = ApprovalRequest(
                approval_id=approval_id,
                title=str(payload.get("title") or ""),
                detail=str(payload.get("detail") or ""),
                timeout_seconds=int(timeout_seconds) if isinstance(timeout_seconds, int) else None,
                channel=str(payload.get("channel") or "") or None,
                urgency=str(payload.get("urgency") or "") or None,
                danger=payload.get("danger") if isinstance(payload.get("danger"), bool) else None,
                options=tuple(str(item) for item in payload.get("options") or ()),
                selected_index=int(payload.get("selected_index") or payload.get("selectedIndex") or 0),
            )
            if content:
                self.current_step = _trim_text(content, 48)
                self.last_event = content
            elif self.pending_approval.title:
                self.current_step = _trim_text(self.pending_approval.title, 48)
                self.last_event = self.pending_approval.title
        elif event_type == "approval_response":
            self.pending_approval = None
            self.status = RunStatus.RUNNING
            if content:
                self.last_event = content
            elif str(payload.get("approved")):
                self.last_event = "approval recorded"
        elif event_type == "codex_delta":
            self.status = RunStatus.RUNNING
            if content:
                self.last_event = content
        elif event_type == "codex_usage":
            if content:
                self.last_event = content
        elif event_type == "codex_status":
            if kind in {"completed", "turn/completed"}:
                self.status = RunStatus.DONE
            elif kind in {"failed", "turn/failed", "error"}:
                self.status = RunStatus.FAILED
            elif kind in {"status", "turn/started", "turn/start/accepted", "thread/status/changed"}:
                self.status = RunStatus.RUNNING
            elif kind == "voice_prompt_transcribed":
                self.status = RunStatus.RUNNING
            elif kind == "session_status":
                return
            elif kind.startswith("bridge_"):
                return
            if content:
                self.last_event = content
            elif kind:
                self.last_event = kind.replace("_", " ")
        elif event_type == "error":
            self.status = RunStatus.FAILED
            if content:
                self.last_event = content
        else:
            if content:
                self.last_event = content

    def to_dict(self) -> dict[str, Any]:
        return {
            "run_id": self.run_id,
            "role": self.role.value,
            "workspace_path": self.workspace_path,
            "base_branch": self.base_branch,
            "worktree_path": self.worktree_path,
            "branch": self.branch,
            "thread_id": self.thread_id,
            "session_id": self.session_id,
            "mode": self.mode.value,
            "status": self.status.value,
            "current_step": self.current_step,
            "last_event": self.last_event,
            "pending_approval": None if self.pending_approval is None else self.pending_approval.to_dict(),
            "diff_summary": None if self.diff_summary is None else self.diff_summary.to_dict(),
            "test_summary": None if self.test_summary is None else self.test_summary.to_dict(),
            "danger_summary": self.danger_summary,
            "badge_mode": self.badge_mode,
            "state_epoch": self.state_epoch,
        }


@dataclass(slots=True)
class RunIndex:
    runs: dict[str, AgentRun] = field(default_factory=dict)
    active_run_id: str | None = None
    _next_run_number: int = 1

    def current(self) -> AgentRun | None:
        if self.active_run_id is None:
            return None
        return self.runs.get(self.active_run_id)

    def select(self, run_id: str) -> AgentRun | None:
        if run_id in self.runs:
            self.active_run_id = run_id
            return self.runs[run_id]
        return None

    def create_run(
        self,
        *,
        workspace_path: str = ".",
        base_branch: str = "main",
        worktree_path: str | None = None,
        branch: str | None = None,
        thread_id: str | None = None,
        session_id: str | None = None,
        role: RunRole = RunRole.MAIN,
        mode: RunMode = RunMode.SAFE,
        status: RunStatus = RunStatus.IDLE,
        current_step: str = "",
        last_event: str = "",
    ) -> AgentRun:
        run_id = self._new_run_id()
        run = AgentRun(
            run_id,
            role=role,
            workspace_path=workspace_path,
            base_branch=base_branch,
            worktree_path=worktree_path,
            branch=branch,
            thread_id=thread_id,
            session_id=session_id,
            mode=mode,
            status=status,
            current_step=current_step,
            last_event=last_event,
        )
        self.runs[run_id] = run
        self.active_run_id = run_id
        return run

    def ensure_active(
        self,
        *,
        workspace_path: str = ".",
        base_branch: str = "main",
        worktree_path: str | None = None,
        branch: str | None = None,
        thread_id: str | None = None,
        session_id: str | None = None,
        role: RunRole = RunRole.MAIN,
        mode: RunMode = RunMode.SAFE,
    ) -> AgentRun:
        run = self.current()
        if run is None:
            return self.create_run(
                workspace_path=workspace_path,
                base_branch=base_branch,
                worktree_path=worktree_path,
                branch=branch,
                thread_id=thread_id,
                session_id=session_id,
                role=role,
                mode=mode,
            )
        run.touch(
            workspace_path=workspace_path,
            base_branch=base_branch,
            worktree_path=worktree_path,
            branch=branch,
            thread_id=thread_id,
            session_id=session_id,
        )
        return run

    def find_by_thread_id(self, thread_id: str) -> AgentRun | None:
        for run in self.ordered_runs():
            if run.thread_id == thread_id:
                return run
        return None

    def find_by_session_id(self, session_id: str) -> AgentRun | None:
        for run in self.ordered_runs():
            if run.session_id == session_id:
                return run
        return None

    def bind_session(self, run_id: str, session_id: str) -> AgentRun | None:
        run = self.runs.get(run_id)
        if run is None:
            return None
        run.session_id = session_id
        return run

    def record(self, events: list[Any], run_id: str | None = None) -> None:
        run = self.runs.get(run_id) if run_id is not None else self.current()
        if run is None:
            run = self.create_run()
        for event in events:
            run.record_event(event)

    def ordered_runs(self) -> list[AgentRun]:
        return sorted(self.runs.values(), key=lambda run: run.run_id, reverse=True)

    def to_dict(self) -> dict[str, Any]:
        runs = {run_id: run.to_dict() for run_id, run in self.runs.items()}
        return {
            "active_run_id": self.active_run_id,
            "runs": runs,
        }

    def _new_run_id(self) -> str:
        run_id = f"run-{self._next_run_number:06d}"
        self._next_run_number += 1
        return run_id
