from __future__ import annotations

import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Any, NamedTuple


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
                ["git"] + args,
                cwd=cwd or self.repo_path,
                capture_output=True,
                text=True,
                check=True,
            )
            return result.stdout.strip()
        except subprocess.CalledProcessError as e:
            raise WorktreeError(f"Git error: {e.stderr.strip() or e.stdout.strip()}") from e

    def list_worktrees(self) -> list[WorktreeInfo]:
        output = self._run_git(["worktree", "list", "--porcelain"])
        worktrees = []
        current_path = None
        current_commit = None
        current_branch = None

        for line in output.splitlines():
            if line.startswith("worktree "):
                path = Path(line[len("worktree "):].strip())
                worktrees.append(WorktreeInfo(path, current_commit or "", current_branch or ""))
                current_path = path
                current_commit = None
                current_branch = None
            elif line.startswith("commit "):
                current_commit = line[len("commit "):].strip()
            elif line.startswith("branch "):
                current_branch = line[len("branch "):].strip()
        
        # The porcelain output puts worktree line FIRST, then commit/branch.
        # I need to parse it as blocks.
        
        # Let's refine the parsing logic.
        return self._parse_porcelain(output)

    def _parse_porcelain(self, output: str) -> list[WorktreeInfo]:
        worktrees = []
        blocks = output.split("\n\n") if output else []
        for block in blocks:
            if not block.strip():
                continue
            lines = block.splitlines()
            path = None
            commit = None
            branch = None
            for line in lines:
                if line.startswith("worktree "):
                    path = Path(line[len("worktree "):].strip())
                elif line.startswith("commit "):
                    commit = line[len("commit "):].strip()
                elif line.startswith("branch "):
                    branch = line[len("branch "):].strip()
            if path:
                worktrees.append(WorktreeInfo(path, commit or "", branch or ""))
        return worktrees

    def create_worktree(self, branch: str, name: str) -> Path:
        # Worktrees are usually created as folders next to the main repo or in a specific place.
        # We'll create them in a 'worktrees' folder inside the repo path's parent or similar.
        # For simplicity, we'll put them in <repo_path>/../worktrees/<name>
        worktree_root = self.repo_path.parent / "worktrees"
        worktree_path = worktree_root / name
        
        if worktree_path.exists():
            raise WorktreeError(f"Worktree path {worktree_path} already exists")

        # Create worktree from branch. If branch doesn't exist, it might fail.
        # We can use 'git worktree add -b <new_branch> <path> <base_branch>'
        # But the requirement says "create worktree from base branch".
        # If we want a new branch for the run:
        new_branch = f"run-{name}"
        self._run_git(["worktree", "add", "-b", new_branch, str(worktree_path), branch])
        return worktree_path

    def remove_worktree(self, path: Path) -> None:
        self._run_git(["worktree", "remove", str(path)])

    def is_protected(self, branch: str) -> bool:
        return branch in self.protected_branches

    def get_worktree_by_name(self, name: str) -> Path | None:
        for wt in self.list_worktrees():
            if wt.path.name == name:
                return wt.path
        return None
