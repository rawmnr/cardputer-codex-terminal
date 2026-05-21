from __future__ import annotations

from typing import Any
from .runs import AgentRun, RunIndex, DiffSummary, TestSummary, RunStatus
from .session import SessionState


def _trim(text: str, limit: int) -> str:
    text = text.strip()
    if len(text) <= limit:
        return text
    return text[: limit - 3] + "..."


class CardputerProjection:
    def __init__(self, run_index: RunIndex):
        self.run_index = run_index
        self.version = "1.0.0"

    def build_status_snapshot(self, session: SessionState) -> dict[str, Any]:
        run = self.run_index.runs.get(session.run_id) if session.run_id else None

        return {
            "type": "status_snapshot",
            "v": self.version,
            "payload": {
                "session_id": session.session_id,
                "title": _trim(session.title, 20),
                "status": session.status,
                "last": _trim(session.last_event, 32),
                "usage": session.codex_usage_percent,
                "run": self._project_run_brief(run) if run else None,
            },
        }

    def build_run_list(self) -> dict[str, Any]:
        runs = self.run_index.ordered_runs()
        return {
            "type": "run_list",
            "v": self.version,
            "payload": {
                "active_run_id": self.run_index.active_run_id,
                "runs": [self._project_run_brief(run) for run in runs],
            },
        }

    def build_run_detail(self, run_id: str) -> dict[str, Any] | None:
        run = self.run_index.runs.get(run_id)
        if not run:
            return None

        return {
            "type": "run_detail",
            "v": self.version,
            "payload": {
                "id": run.run_id,
                "role": run.role.value,
                "mode": run.mode.value,
                "status": run.status.value,
                "branch": run.branch or run.base_branch,
                "step": _trim(run.current_step, 40),
                "last": _trim(run.last_event, 40),
                "thread_id": run.thread_id,
                "session_id": run.session_id,
                "workspace_path": run.workspace_path,
                "diff": self._project_diff(run.diff_summary),
                "test": self._project_test(run.test_summary),
                "danger": run.danger_summary,
                "badge": run.badge_mode,
                "wt": run.worktree_exists,
                "stale": _trim(run.staleness_reason, 24),
                "merge_ready": run.merge_ready,
                "approval": self._project_approval(run),
                "actions": self._project_run_actions(run),
            },
        }

    def build_approval_inbox(self) -> dict[str, Any]:
        runs_with_approval = [run for run in self.run_index.ordered_runs() if run.pending_approval is not None]
        approvals = [self._project_approval(run) for run in runs_with_approval]
        approvals.sort(key=lambda item: (0 if item.get("danger_level") == "high" else 1, item.get("run_id", "")))
        return {
            "type": "approval_inbox",
            "v": self.version,
            "payload": {
                "count": len(approvals),
                "approvals": approvals,
            },
        }

    def _project_run_brief(self, run: AgentRun | None) -> dict[str, Any] | None:
        if run is None:
            return None
        approval = run.pending_approval
        return {
            "id": run.run_id,
            "title": _trim(run.current_step or run.role.value, 20),
            "branch": _trim(run.branch or run.base_branch, 15),
            "mode": run.mode.value,
            "status": run.status.value,
            "last": _trim(run.last_event, 24),
            "thread_id": run.thread_id or "",
            "approval_id": approval.approval_id if approval is not None else "",
            "approval_title": _trim(approval.title, 18) if approval is not None else "",
            "danger_level": "high" if approval is not None and approval.danger else "normal",
            "merge_ready": run.merge_ready,
            "wt": "ok" if run.worktree_exists else "missing",
            "warn": _trim(run.staleness_reason, 16),
        }

    def _project_diff(self, diff: DiffSummary | None) -> dict[str, Any] | None:
        if not diff:
            return None
        return {
            "files": diff.files_changed,
            "ins": diff.insertions,
            "del": diff.deletions,
            "sum": _trim(diff.summary, 32),
        }

    def _project_test(self, test: TestSummary | None) -> dict[str, Any] | None:
        if not test:
            return None
        return {
            "run": test.tests_run,
            "pass": test.passed,
            "fail": test.failed,
            "skip": test.skipped,
            "sum": _trim(test.summary, 32),
        }

    def _project_run_actions(self, run: AgentRun) -> list[dict[str, Any]]:
        actions: list[dict[str, Any]] = []
        if run.pending_approval is not None:
            actions.append({"id": "approve_once", "label": "Approve once"})
            actions.append({"id": "reject", "label": "Reject"})
            actions.append({"id": "stop", "label": "Stop"})
        elif run.status in {RunStatus.RUNNING, RunStatus.PAUSED}:
            actions.append({"id": "stop", "label": "Stop"})
        if run.diff_summary is not None:
            actions.append({"id": "show_diff", "label": "Show diff"})
        if run.test_summary is not None:
            actions.append({"id": "show_tests", "label": "Show tests"})
        if run.thread_id:
            actions.append({"id": "open_voice_reply", "label": "Open voice reply"})
        if run.status in {RunStatus.DONE, RunStatus.FAILED}:
            actions.append({"id": "mark_for_merge", "label": "Mark for merge"})
        return actions

    def _project_approval(self, run: AgentRun) -> dict[str, Any]:
        req = run.pending_approval
        assert req is not None
        return {
            "run_id": run.run_id,
            "run_title": _trim(run.current_step or run.role.value, 20),
            "id": req.approval_id,
            "title": _trim(req.title, 32),
            "detail": _trim(req.detail, 64),
            "mode": run.mode.value,
            "status": run.status.value,
            "branch": _trim(run.branch or run.base_branch, 15),
            "thread_id": run.thread_id or "",
            "danger_level": "high" if req.danger else "normal",
            "options": list(req.options),
            "danger": req.danger,
        }
