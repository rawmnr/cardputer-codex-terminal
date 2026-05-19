from __future__ import annotations

import json
import tempfile
from dataclasses import dataclass
from pathlib import Path

from .runs import AgentRun, RunIndex, RunStatus
from .worktree import WorktreeError, WorktreeManager

STATE_VERSION = 1
DEFAULT_STATE_PATH = Path.home() / ".cardputer-codex" / "runs.json"


@dataclass(slots=True)
class PersistenceReport:
    loaded_runs: int = 0
    stale_runs: int = 0
    missing_worktrees: int = 0
    removed_runs: int = 0


class RunPersistenceStore:
    def __init__(self, path: Path | str | None = None) -> None:
        self.enabled = path is not None
        self.path = Path(path) if path is not None else DEFAULT_STATE_PATH

    def load(self, worktree_manager: WorktreeManager | None = None) -> tuple[RunIndex, PersistenceReport]:
        if not self.enabled or not self.path.exists():
            return RunIndex(), PersistenceReport()

        try:
            raw = json.loads(self.path.read_text(encoding="utf-8"))
        except Exception:
            return RunIndex(), PersistenceReport()

        if not isinstance(raw, dict):
            return RunIndex(), PersistenceReport()

        index = RunIndex.from_persisted_dict(raw)
        report = self.reconcile(index, worktree_manager, pause_running=True)
        report.loaded_runs = len(index.runs)
        return index, report

    def reconcile(
        self,
        run_index: RunIndex,
        worktree_manager: WorktreeManager | None = None,
        *,
        pause_running: bool = False,
    ) -> PersistenceReport:
        report = PersistenceReport(loaded_runs=len(run_index.runs))
        if not self.enabled:
            return report

        for run in run_index.runs.values():
            stale_reasons: list[str] = []

            if pause_running and run.status in {RunStatus.RUNNING, RunStatus.APPROVAL}:
                run.status = RunStatus.PAUSED
                stale_reasons.append("codex state paused after restart")
                report.stale_runs += 1

            if run.worktree_path:
                worktree_exists = Path(run.worktree_path).exists()
            elif worktree_manager is not None and run.branch:
                try:
                    worktree_exists = any(wt.branch == run.branch for wt in worktree_manager.list_worktrees())
                except WorktreeError:
                    worktree_exists = True
            else:
                worktree_exists = True

            run.worktree_exists = worktree_exists
            if not worktree_exists:
                report.missing_worktrees += 1
                stale_reasons.append("worktree missing")

            run.staleness_reason = "; ".join(stale_reasons)

        if run_index.active_run_id and run_index.active_run_id not in run_index.runs:
            run_index.active_run_id = None
        return report

    def save(self, run_index: RunIndex) -> None:
        if not self.enabled:
            return
        self.path.parent.mkdir(parents=True, exist_ok=True)
        payload = {
            "version": STATE_VERSION,
            **run_index.to_persisted_dict(),
        }
        data = json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True)
        with tempfile.NamedTemporaryFile(
            "w",
            encoding="utf-8",
            delete=False,
            dir=self.path.parent,
            prefix=self.path.name + ".",
            suffix=".tmp",
        ) as tmp:
            tmp.write(data)
            tmp_path = Path(tmp.name)
        tmp_path.replace(self.path)

    def cleanup_orphans(self, run_index: RunIndex) -> PersistenceReport:
        report = PersistenceReport(loaded_runs=len(run_index.runs))
        if not self.enabled:
            return report

        keep: dict[str, AgentRun] = {}
        removed = 0
        for run_id, run in run_index.runs.items():
            missing_worktree = bool(run.worktree_path) and not Path(run.worktree_path).exists()
            removable = missing_worktree and run.status in {RunStatus.PAUSED, RunStatus.IDLE, RunStatus.DONE, RunStatus.FAILED}
            if removable:
                removed += 1
                continue
            keep[run_id] = run
        run_index.runs = keep
        if run_index.active_run_id not in run_index.runs:
            run_index.active_run_id = next(iter(run_index.runs), None)
        report.removed_runs = removed
        return report
