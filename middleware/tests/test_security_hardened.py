import pytest
from cardputer_codex_terminal.policies import ApprovalPolicyManager, ApprovalMode, RunMode, ApprovalPolicy
from cardputer_codex_terminal.config import AppConfig

def test_yolo_branch_protection():
    manager = ApprovalPolicyManager()
    yolo_policy = manager.get_policy(RunMode.YOLO_WORKTREE)
    
    # Mock request on a sensitive branch (logic to be implemented in evaluate_request)
    request_data = {
        "command": "rm -rf /",
        "branch": "main"
    }
    
    # We expect this to be rejected or require user approval even in YOLO
    approved, reason = manager.evaluate_request(yolo_policy, request_data)
    assert approved is not True
    assert "main" in reason.lower() or "forbidden" in reason.lower()

def test_device_rbac():
    manager = ApprovalPolicyManager()
    
    # Mock a device policy that only allows 'status'
    device_policy = {
        "allowed_actions": ["status"],
        "denied_actions": ["approve"]
    }
    
    request_approve = {"action": "approve", "device_id": "limited-device"}
    # This should be rejected by the manager (logic to be added)
    approved, reason = manager.evaluate_request_for_device(device_policy, request_approve)
    assert approved is False
    assert "not allowed" in reason.lower() or "denied" in reason.lower()
