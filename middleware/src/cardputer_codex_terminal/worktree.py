from __future__ import annotations

import re
import subprocess
from pathlib import Path
from typing import NamedTuple

from .runs import AgentRun, DiffSummary, TestSummary


class WorktreeInfo(NamedTuple):
    path: Path
    commit: str
    branch: str


class WorktreeError(Exception):
    pass


class WorktreeManager:
    def __init__(self, repo_path: Path):
        self.repo_path = repo_path
        self.protected_branches = {"main", "preprod", "prod"}

    def _run_git(self, args: list[str], cwd: Path | None = None) -> str:
        try:
            result = subprocess.run(
                ["git", *args],
                cwd=cwd or self.repo_path,
                capture_output=True,
                text=True,
                check=True,
            )
            return result.stdout.strip()
        except subprocess.CalledProcessError as exc:
            raise WorktreeError(f"Git error: {exc.stderr.strip() or exc.stdout.strip()}") from exc

    def normalize_branch_name(self, branch: str) -> str:
        value = branch.strip()
        for prefix in ("refs/heads/", "refs/remotes/", "origin/"):
            if value.startswith(prefix):
                value = value[len(prefix) :]
        return value

    def list_worktrees(self) -> list[WorktreeInfo]:
        output = self._run_git(["worktree", "list", "--porcelain"])
        return self._parse_porcelain(output)

    def _parse_porcelain(self, output: str) -> list[WorktreeInfo]:
        worktrees: list[WorktreeInfo] = []
        for block in output.split("\n\n") if output else []:
            if not block.strip():
                continue
            path = ""
            commit = ""
            branch = ""
            for line in block.splitlines():
                if line.startswith("worktree "):
                    path = line[len("worktree ") :].strip()
                elif line.startswith("commit "):
                    commit = line[len("commit ") :].strip()
                elif line.startswith("branch "):
                    branch = self.normalize_branch_name(line[len("branch ") :].strip())
            if path:
                worktrees.append(WorktreeInfo(Path(path), commit, branch))
        return worktrees

    def get_worktree_info_by_name(self, name: str) -> WorktreeInfo | None:
        for worktree in self.list_worktrees():
            if worktree.path.name == name:
                return worktree
        return None

    def get_worktree_info_by_path(self, path: Path) -> WorktreeInfo | None:
        for worktree in self.list_worktrees():
            if worktree.path == path:
                return worktree
        return None

    def create_worktree(self, branch: str, name: str) -> Path:
        worktree_root = self.repo_path.parent / "worktrees"
        worktree_path = worktree_root / name
        if worktree_path.exists():
            raise WorktreeError(f"Worktree path {worktree_path} already exists")
        new_branch = f"run-{name}"
        self._run_git(["worktree", "add", "-b", new_branch, str(worktree_path), branch])
        return worktree_path

    def remove_worktree(self, path: Path) -> None:
        self._run_git(["worktree", "remove", str(path)])

    def is_protected(self, branch: str) -> bool:
        return self.normalize_branch_name(branch) in self.protected_branches

    def get_worktree_by_name(self, name: str) -> Path | None:
        info = self.get_worktree_info_by_name(name)
        return info.path if info is not None else None

    def collect_diff(self, path: Path, base_ref: str | None = None) -> DiffSummary:
        args = ["-C", str(path), "diff"]
        if base_ref:
            args.append(base_ref)
        shortstat = self._run_git([*args, "--shortstat"], cwd=self.repo_path)
        names = self._run_git([*args, "--name-only"], cwd=self.repo_path)
        files = [line for line in names.splitlines() if line.strip()]
        files_changed = len(files)
        insertions = 0
        deletions = 0
        if shortstat:
            insertions_match = re.search(r"(\d+) insertions?\(\+\)", shortstat)
            deletions_match = re.search(r"(\d+) deletions?\(-\)", shortstat)
            files_match = re.search(r"(\d+) files? changed", shortstat)
            if insertions_match:
                insertions = int(insertions_match.group(1))
            if deletions_match:
                deletions = int(deletions_match.group(1))
            if files_match:
                files_changed = int(files_match.group(1))
        summary_bits = [bit for bit in [shortstat, ", ".join(files[:5])] if bit]
        summary = "; ".join(summary_bits)
        return DiffSummary(files_changed=files_changed, insertions=insertions, deletions=deletions, summary=summary)

    def run_tests(self, path: Path, command: list[str] | None = None) -> TestSummary:
        if command is None:
            if not (path / "pyproject.toml").exists() or not (path / "tests").exists():
                return TestSummary(summary="No test command configured for this worktree.")
            command = ["uv", "run", "python", "-m", "unittest", "discover", "-s", "tests", "-v"]
        result = subprocess.run(command, cwd=path, capture_output=True, text=True, check=False)
        stdout = result.stdout.strip()
        stderr = result.stderr.strip()
        combined = "\n".join(part for part in [stdout, stderr] if part)
        tests_run = 0
        failed = 0
        skipped = 0
        run_match = re.search(r"Ran (\d+) tests?", combined)
        if run_match:
            tests_run = int(run_match.group(1))
        skip_match = re.search(r"skipped=(\d+)", combined)
        if skip_match:
            skipped = int(skip_match.group(1))
        fail_match = re.search(r"FAILED \(([^)]+)\)", combined)
        if fail_match:
            failure_counts = re.findall(r"(failures|errors)=([0-9]+)", fail_match.group(1))
            failed = sum(int(count) for _, count in failure_counts)
        elif result.returncode != 0:
            failed = 1 if tests_run == 0 else max(1, failed)
        passed = max(0, tests_run - failed - skipped)
        summary = combined.splitlines()[-1] if combined else "Tests completed."
        return TestSummary(tests_run=tests_run, passed=passed, failed=failed, skipped=skipped, summary=summary)

    def generate_merge_report(self, run: AgentRun) -> str:
        branch = self.normalize_branch_name(run.branch or run.base_branch)
        worktree = run.worktree_path or run.workspace_path
        diff = run.diff_summary.summary if run.diff_summary is not None else "No diff collected"
        tests = run.test_summary.summary if run.test_summary is not None else "No tests collected"
        merge_ready = "yes" if run.merge_ready else "no"
        lines = [
            f"Run: {run.run_id}",
            f"Branch: {branch}",
            f"Worktree: {worktree}",
            f"Mode: {run.mode.value}",
            f"Status: {run.status.value}",
            f"Merge ready: {merge_ready}",
            f"Diff: {diff}",
            f"Tests: {tests}",
            f"Last event: {run.last_event or 'n/a'}",
        ]
        return "\n".join(lines)
