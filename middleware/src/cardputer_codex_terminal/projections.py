from __future__ import annotations

from typing import Any

from .runs import AgentRun, DiffSummary, RunIndex, RunStatus, TestSummary
from .session import SessionIndex, SessionState


def _trim(text: str, limit: int) -> str:
    text = text.strip()
    if len(text) <= limit:
        return text
    return text[: limit - 3] + "..."


class CardputerProjection:
    def __init__(self, session_index: SessionIndex, run_index: RunIndex):
        self.session_index = session_index
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

    def build_session_status(self, session: SessionState) -> dict[str, Any]:
        sessions = []
        for item in self.session_index.ordered_sessions()[:10]:
            snapshot = item.to_dict()
            if isinstance(snapshot.get("bridge_prompt_options"), tuple):
                snapshot["bridge_prompt_options"] = list(snapshot["bridge_prompt_options"])
            if "events" in snapshot and isinstance(snapshot["events"], list):
                pruned_events = []
                for event in snapshot["events"][-5:]:
                    event_type = event.get("type", "")
                    event_payload = event.get("payload", {})
                    payload: dict[str, Any] = {}
                    for key in ("content", "text", "message", "kind"):
                        if key in event_payload:
                            payload[key] = event_payload[key]
                    pruned_events.append({"type": event_type, "payload": payload})
                snapshot["events"] = pruned_events
            sessions.append(snapshot)

        runs = [run.to_dict() for run in self.run_index.ordered_runs()[:10]]
        return {
            "type": "session_status",
            "v": self.version,
            "payload": {
                "kind": "session_status",
                "active_session_id": self.session_index.active_session_id,
                "active_run_id": self.run_index.active_run_id,
                "workspace_path": session.workspace_path,
                "branch": session.branch,
                "thread_id": session.thread_id,
                "title": session.title,
                "status": session.status,
                "last_event": session.last_event,
                "state_epoch": session.state_epoch,
                "codex_usage_percent": session.codex_usage_percent,
                "codex_usage_secondary_percent": session.codex_usage_secondary_percent,
                "codex_usage_window_minutes": session.codex_usage_window_minutes,
                "codex_usage_secondary_window_minutes": session.codex_usage_secondary_window_minutes,
                "codex_usage_resets_at": session.codex_usage_resets_at,
                "codex_usage_secondary_resets_at": session.codex_usage_secondary_resets_at,
                "codex_usage_reset_line": session.codex_usage_reset_line,
                "approval_id": session.pending_approval_id,
                "approval_title": session.pending_approval_title,
                "approval_detail": session.pending_approval_detail,
                "sessions": sessions,
                "runs": runs,
                "interrupt_supported": True,
                "bridge_prompt_kind": session.bridge_prompt_kind,
                "bridge_prompt_title": session.bridge_prompt_title,
                "bridge_prompt_detail": session.bridge_prompt_detail,
                "bridge_prompt_channel": session.bridge_prompt_channel,
                "bridge_prompt_urgency": session.bridge_prompt_urgency,
                "bridge_prompt_danger": session.bridge_prompt_danger,
                "bridge_prompt_options": list(session.bridge_prompt_options),
                "bridge_prompt_selected_index": session.bridge_prompt_selected_index,
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
                "approval": self._project_approval(run) if run.pending_approval is not None else None,
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
        else:
            if run.status == RunStatus.RUNNING:
                actions.append({"id": "pause_run", "label": "Pause"})
                actions.append({"id": "stop", "label": "Stop"})
            elif run.status == RunStatus.PAUSED:
                actions.append({"id": "resume_run", "label": "Resume"})
                actions.append({"id": "stop", "label": "Stop"})
        if run.worktree_path:
            actions.append({"id": "collect_diff", "label": "Diff"})
            actions.append({"id": "run_tests", "label": "Tests"})
            actions.append({"id": "generate_merge_report", "label": "Report"})
        if run.thread_id:
            actions.append({"id": "open_voice_reply", "label": "Voice reply"})
        if run.status in {RunStatus.DONE, RunStatus.FAILED}:
            actions.append({"id": "mark_for_merge", "label": "Mark merge"})
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
