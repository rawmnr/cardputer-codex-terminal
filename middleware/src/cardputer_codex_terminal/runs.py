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

def _coerce_int(value: Any) -> int:
    if isinstance(value, bool):
        return 0
    try:
        return int(value)
    except (TypeError, ValueError):
        return 0



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
    worktree_exists: bool = True
    staleness_reason: str = ""
    merge_ready: bool = False

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

    def _apply_diff_summary(self, data: Any) -> None:
        if not isinstance(data, dict):
            return
        self.diff_summary = DiffSummary(
            files_changed=_coerce_int(data.get("files_changed") or data.get("files")),
            insertions=_coerce_int(data.get("insertions") or data.get("ins")),
            deletions=_coerce_int(data.get("deletions") or data.get("del")),
            summary=str(data.get("summary") or data.get("sum") or data.get("text") or data.get("content") or ""),
        )

    def _apply_test_summary(self, data: Any) -> None:
        if not isinstance(data, dict):
            return
        self.test_summary = TestSummary(
            tests_run=_coerce_int(data.get("tests_run") or data.get("run") or data.get("tests")),
            passed=_coerce_int(data.get("passed") or data.get("pass")),
            failed=_coerce_int(data.get("failed") or data.get("fail")),
            skipped=_coerce_int(data.get("skipped") or data.get("skip")),
            summary=str(data.get("summary") or data.get("sum") or data.get("text") or data.get("content") or ""),
        )

    def record_event(self, event: Any) -> None:
        self.state_epoch += 1
        event_type, payload = _event_payload(event)
        event_dict = event.to_dict() if hasattr(event, "to_dict") else event if isinstance(event, dict) else {"type": event_type, "payload": payload}
        if not isinstance(event_dict, dict):
            return

        kind = str(payload.get("kind") or "")
        content = str(payload.get("content") or payload.get("text") or payload.get("message") or "")
        diff_summary = payload.get("diff_summary") or payload.get("diff")
        if diff_summary is not None:
            self._apply_diff_summary(diff_summary)
        test_summary = payload.get("test_summary") or payload.get("test")
        if test_summary is not None:
            self._apply_test_summary(test_summary)

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
            elif kind in {"status", "turn/started", "turn/start/accepted", "thread/status/changed", "run_started", "run_resumed", "resume_run"}:
                self.status = RunStatus.RUNNING
            elif kind in {"run_paused", "pause_run"}:
                self.status = RunStatus.PAUSED
            elif kind == "voice_prompt_transcribed":
                self.status = RunStatus.RUNNING
            elif kind == "session_status":
                return
            elif kind.startswith("bridge_"):
                return
            elif kind in {"run_stopped", "stopped"}:
                self.status = RunStatus.PAUSED
            elif kind in {"run_marked_for_merge", "merge_ready", "mark_ready_for_merge"}:
                self.merge_ready = True
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
            "worktree_exists": self.worktree_exists,
            "staleness_reason": self.staleness_reason,
            "merge_ready": self.merge_ready,
            "state_epoch": self.state_epoch,
        }

    def to_persisted_dict(self) -> dict[str, Any]:
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
            "diff_summary": None if self.diff_summary is None else self.diff_summary.to_dict(),
            "test_summary": None if self.test_summary is None else self.test_summary.to_dict(),
            "danger_summary": self.danger_summary,
            "badge_mode": self.badge_mode,
            "worktree_exists": self.worktree_exists,
            "staleness_reason": self.staleness_reason,
            "merge_ready": self.merge_ready,
            "state_epoch": self.state_epoch,
        }

    @classmethod
    def from_persisted_dict(cls, data: dict[str, Any]) -> "AgentRun":
        run = cls(
            str(data.get("run_id") or ""),
            role=RunRole(str(data.get("role") or RunRole.MAIN.value)),
            workspace_path=str(data.get("workspace_path") or "."),
            base_branch=str(data.get("base_branch") or "main"),
            worktree_path=str(data.get("worktree_path") or "") or None,
            branch=str(data.get("branch") or "") or None,
            thread_id=str(data.get("thread_id") or "") or None,
            session_id=str(data.get("session_id") or "") or None,
            mode=RunMode(str(data.get("mode") or RunMode.SAFE.value)),
            status=RunStatus(str(data.get("status") or RunStatus.IDLE.value)),
            danger_summary=str(data.get("danger_summary") or ""),
            badge_mode=str(data.get("badge_mode") or "SAFE"),
            worktree_exists=bool(data.get("worktree_exists", True)),
            staleness_reason=str(data.get("staleness_reason") or ""),
            merge_ready=bool(data.get("merge_ready", False)),
            state_epoch=int(data.get("state_epoch") or 0),
        )
        diff_summary = data.get("diff_summary")
        if isinstance(diff_summary, dict):
            run.diff_summary = DiffSummary(
                files_changed=int(diff_summary.get("files_changed") or 0),
                insertions=int(diff_summary.get("insertions") or 0),
                deletions=int(diff_summary.get("deletions") or 0),
                summary=str(diff_summary.get("summary") or ""),
            )
        test_summary = data.get("test_summary")
        if isinstance(test_summary, dict):
            run.test_summary = TestSummary(
                tests_run=int(test_summary.get("tests_run") or 0),
                passed=int(test_summary.get("passed") or 0),
                failed=int(test_summary.get("failed") or 0),
                skipped=int(test_summary.get("skipped") or 0),
                summary=str(test_summary.get("summary") or ""),
            )
        return run


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

    def find_by_approval_id(self, approval_id: str) -> AgentRun | None:
        for run in self.ordered_runs():
            if run.pending_approval is not None and run.pending_approval.approval_id == approval_id:
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

    def to_persisted_dict(self) -> dict[str, Any]:
        return {
            "active_run_id": self.active_run_id,
            "runs": [run.to_persisted_dict() for run in self.ordered_runs()],
        }

    @classmethod
    def from_persisted_dict(cls, data: dict[str, Any]) -> "RunIndex":
        runs_data = data.get("runs")
        runs: dict[str, AgentRun] = {}
        next_run_number = 1
        if isinstance(runs_data, list):
            for item in runs_data:
                if not isinstance(item, dict):
                    continue
                run = AgentRun.from_persisted_dict(item)
                if not run.run_id:
                    continue
                runs[run.run_id] = run
                if run.run_id.startswith("run-"):
                    try:
                        next_run_number = max(next_run_number, int(run.run_id.split("-", 1)[1]) + 1)
                    except ValueError:
                        pass
        return cls(runs=runs, active_run_id=str(data.get("active_run_id") or "") or None, _next_run_number=next_run_number)

    def _new_run_id(self) -> str:
        run_id = f"run-{self._next_run_number:06d}"
        self._next_run_number += 1
        return run_id
