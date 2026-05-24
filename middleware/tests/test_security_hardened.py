from __future__ import annotations

import unittest

from cardputer_codex_terminal.messages import CardputerMessageType
from cardputer_codex_terminal.policies import ApprovalPolicyManager, RunMode


class SecurityHardenedTests(unittest.TestCase):
    def test_yolo_branch_protection(self) -> None:
        manager = ApprovalPolicyManager()
        yolo_policy = manager.get_policy(RunMode.YOLO_WORKTREE)

        request_data = {
            "command": "rm -rf /",
            "branch": "main",
        }

        approved, reason = manager.evaluate_request(yolo_policy, request_data)
        self.assertIsNot(approved, True)
        self.assertTrue("main" in reason.lower() or "forbidden" in reason.lower())

    def test_device_rbac(self) -> None:
        manager = ApprovalPolicyManager()
        device_policy = {
            "allowed_message_types": ["status_request"],
            "denied_message_types": ["approval_response"],
        }

        approved, reason = manager.evaluate_request_for_device(
            device_policy,
            {"message_type": "approval_response", "device_id": "limited-device"},
        )
        self.assertFalse(approved)
        self.assertTrue("not allowed" in reason.lower() or "denied" in reason.lower())

    def test_device_rbac_allows_legitimate_cardputer_messages(self) -> None:
        manager = ApprovalPolicyManager()
        device_policy = {
            "allowed_message_types": [m.value for m in CardputerMessageType if m != CardputerMessageType.HELLO_ACK],
        }

        for message_type in (
            CardputerMessageType.TEXT_PROMPT,
            CardputerMessageType.AUDIO_CHUNK,
            CardputerMessageType.VOICE_PROMPT_READY,
            CardputerMessageType.RUN_LIST_REQUEST,
            CardputerMessageType.RUN_DETAIL_REQUEST,
            CardputerMessageType.APPROVAL_INBOX_REQUEST,
            CardputerMessageType.BRIDGE_RESPONSE,
        ):
            with self.subTest(message_type=message_type.value):
                approved, reason = manager.evaluate_request_for_device(device_policy, {"message_type": message_type.value})
                self.assertIsNone(approved)
                self.assertIn("passed", reason.lower())
