from __future__ import annotations

from dataclasses import dataclass, field
from enum import StrEnum
from typing import Any, Literal

from .runs import RunMode


class ApprovalMode(StrEnum):
    USER = "user"
    NEVER = "never"


@dataclass(slots=True, frozen=True)
class ApprovalPolicy:
    mode: RunMode
    approval_mode: ApprovalMode = ApprovalMode.USER
    allow_write: bool = False
    allow_exec: bool = False
    allow_git_push: bool = False
    scope: Literal["global", "worktree_only"] = "global"
    forbidden_paths: list[str] = field(default_factory=list)
    forbidden_branches: list[str] = field(default_factory=lambda: ["main", "master", "prod", "production", "preprod"])
    auto_revert_on_failure: bool = False
    require_physical_confirmation: bool = False

SAFE_POLICY = ApprovalPolicy(
    mode=RunMode.SAFE,
    approval_mode=ApprovalMode.USER,
    allow_write=True,
    allow_exec=True,
    allow_git_push=False,
    require_physical_confirmation=True,
)

YOLO_WORKTREE_POLICY = ApprovalPolicy(
    mode=RunMode.YOLO_WORKTREE,
    approval_mode=ApprovalMode.NEVER,
    allow_write=True,
    allow_exec=True,
    allow_git_push=False,
    scope="worktree_only",
    forbidden_paths=[
        ".git/config",
        ".env",
        "secrets/",
        "production/",
    ],
    auto_revert_on_failure=True,
    require_physical_confirmation=True,
)

REVIEW_ONLY_POLICY = ApprovalPolicy(
    mode=RunMode.REVIEW_ONLY,
    approval_mode=ApprovalMode.NEVER,
    allow_write=False,
    allow_exec=False,
    require_physical_confirmation=True,
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

    def evaluate_request_for_device(self, device_policy: dict[str, Any], request_data: dict[str, Any]) -> tuple[bool | None, str]:
        """Checks if a specific device is allowed to perform the requested action."""
        action = request_data.get("action") or request_data.get("command") or request_data.get("title")
        if not action:
            return None, "No action/command specified, skipping device check."

        allowed = device_policy.get("allowed_actions", [])
        denied = device_policy.get("denied_actions", [])

        action_str = str(action).lower()
        if any(kw.lower() in action_str for kw in denied):
            return False, f"Action '{action}' is explicitly denied for this device."

        if allowed and not any(kw.lower() in action_str for kw in allowed):
            return False, f"Action '{action}' is not in the allowed list for this device."

        return None, "Device-level check passed."


    def evaluate_request(self, policy: ApprovalPolicy, request_data: dict[str, Any]) -> tuple[bool | None, str]:
        """
        Evaluates an approval request against a policy.
        Returns (approved, reason).
        If approved is None, it means the policy requires user intervention.
        """
        title = str(request_data.get("title") or request_data.get("summary") or request_data.get("command") or "").lower()
        detail = str(request_data.get("detail") or "").lower()
        branch = str(request_data.get("branch") or "").lower()

        # Branch protection
        if branch in policy.forbidden_branches:
            return False, f"Operations on branch '{branch}' always require manual approval (Policy: {policy.mode})."

        # Destructive action physical confirmation check
        if policy.require_physical_confirmation and self.is_destructive(request_data):
             return None, "Destructive action requires physical confirmation on Cardputer."

        # Determine danger category/action type
        is_write = any(kw in title or kw in detail for kw in ["write", "create", "delete", "update", "modify", "edit", "save", "rm ", "remove"])
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

    def is_destructive(self, request_data: dict[str, Any]) -> bool:
        title = str(request_data.get("title") or request_data.get("command") or "").lower()
        detail = str(request_data.get("detail") or "").lower()
        destructive_keywords = ["delete", "rm ", "remove", "wipe", "format", "drop table", "truncate"]
        return any(kw in title or kw in detail for kw in destructive_keywords)
