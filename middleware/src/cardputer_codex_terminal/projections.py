from __future__ import annotations

from typing import Any
from .runs import AgentRun, RunIndex, DiffSummary, TestSummary
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
                "diff": self._project_diff(run.diff_summary),
                "test": self._project_test(run.test_summary),
                "danger": run.danger_summary,
                "badge": run.badge_mode,
            },
        }

    def build_approval_inbox(self) -> dict[str, Any]:
        runs_with_approval = [run for run in self.run_index.runs.values() if run.pending_approval is not None]

        return {
            "type": "approval_inbox",
            "v": self.version,
            "payload": {
                "count": len(runs_with_approval),
                "approvals": [self._project_approval(run) for run in runs_with_approval],
            },
        }

    def _project_run_brief(self, run: AgentRun) -> dict[str, Any]:
        return {
            "id": run.run_id,
            "title": _trim(run.current_step or run.role.value, 20),
            "branch": _trim(run.branch or run.base_branch, 15),
            "mode": run.mode.value,
            "status": run.status.value,
            "last": _trim(run.last_event, 24),
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

    def _project_approval(self, run: AgentRun) -> dict[str, Any]:
        req = run.pending_approval
        assert req is not None
        return {
            "run_id": run.run_id,
            "id": req.approval_id,
            "title": _trim(req.title, 32),
            "detail": _trim(req.detail, 64),
            "options": list(req.options),
            "danger": req.danger,
        }
