from __future__ import annotations

from dataclasses import dataclass, field
from enum import StrEnum
from typing import Any

from .runs import RunMode


class ApprovalMode(StrEnum):
    USER = "user"
    NEVER = "never"


@dataclass(slots=True, frozen=True)
class ApprovalPolicy:
    mode: RunMode
    approval_mode: ApprovalMode
    allow_write: bool = True
    allow_exec: bool | str = "ask"
    allow_git_push: bool = False
    scope: str | None = None
    forbidden_paths: tuple[str, ...] = field(default_factory=tuple)
    require_cardputer_confirm_for: tuple[str, ...] = field(default_factory=tuple)
    auto_revert_on_failure: bool = False


SAFE_POLICY = ApprovalPolicy(
    mode=RunMode.SAFE,
    approval_mode=ApprovalMode.USER,
    allow_write=True,
    allow_exec="ask",
    allow_git_push=False,
    require_cardputer_confirm_for=(
        "git push",
        "rm",
        "git clean",
        "docker compose down",
        "secrets access",
    ),
)

YOLO_WORKTREE_POLICY = ApprovalPolicy(
    mode=RunMode.YOLO_WORKTREE,
    approval_mode=ApprovalMode.NEVER,
    allow_write=True,
    allow_exec=True,
    allow_git_push=False,
    scope="worktree_only",
    forbidden_paths=(
        ".git/config",
        ".env",
        "secrets/",
        "production/",
    ),
    auto_revert_on_failure=True,
)

REVIEW_ONLY_POLICY = ApprovalPolicy(
    mode=RunMode.REVIEW_ONLY,
    approval_mode=ApprovalMode.NEVER,
    allow_write=False,
    allow_exec=False,
)


class ApprovalPolicyManager:
    def __init__(self) -> None:
        self.policies = {
            RunMode.SAFE: SAFE_POLICY,
            RunMode.YOLO_WORKTREE: YOLO_WORKTREE_POLICY,
            RunMode.REVIEW_ONLY: REVIEW_ONLY_POLICY,
        }

    def get_policy(self, mode: RunMode) -> ApprovalPolicy:
        return self.policies.get(mode, SAFE_POLICY)

    def evaluate_request(self, policy: ApprovalPolicy, request_data: dict[str, Any]) -> tuple[bool | None, str]:
        """
        Evaluates an approval request against a policy.
        Returns (approved, reason).
        If approved is None, it means the policy requires user intervention.
        """
        title = str(request_data.get("title") or request_data.get("summary") or request_data.get("command") or "").lower()
        detail = str(request_data.get("detail") or "").lower()
        
        # Determine danger category/action type
        is_write = any(kw in title or kw in detail for kw in ["write", "create", "delete", "update", "modify", "edit", "save"])
        is_exec = any(kw in title or kw in detail for kw in ["execute", "run", "command", "bash", "shell"])
        is_git_push = "git push" in title or "git push" in detail
        
        # Check against review_only
        if not policy.allow_write and is_write:
            return False, "Policy REVIEW_ONLY forbids write operations."
        if not policy.allow_exec and is_exec:
            return False, "Policy REVIEW_ONLY forbids execution."
            
        # Check git push
        if not policy.allow_git_push and is_git_push:
            return False, f"Policy {policy.mode} forbids git push."
            
        # Check forbidden paths in YOLO
        if policy.scope == "worktree_only":
            # Block directory traversal or absolute paths if they look suspicious
            if is_write or is_exec:
                if ".." in title or ".." in detail:
                    return False, f"Policy {policy.mode} forbids directory traversal (..) in worktree_only scope."
                # Simple check for absolute paths on Unix-like or Windows
                if title.startswith("/") or detail.startswith("/") or ":\\" in title or ":\\" in detail:
                    return False, f"Policy {policy.mode} forbids absolute paths in worktree_only scope."

            for path in policy.forbidden_paths:
                if path in title or path in detail:
                    return False, f"Policy {policy.mode} forbids access to {path}."

        # Handle approval_mode
        if policy.approval_mode == ApprovalMode.NEVER:
            # In YOLO/REVIEW, if we haven't rejected yet, we auto-approve if allowed
            if is_exec and policy.allow_exec is False:
                return False, f"Policy {policy.mode} forbids execution."
            return True, f"Policy {policy.mode} auto-approves."

        # Default to user intervention
        return None, "Policy requires user approval."
