import unittest
from cardputer_codex_terminal.policies import ApprovalPolicyManager, RunMode, SAFE_POLICY, YOLO_WORKTREE_POLICY, REVIEW_ONLY_POLICY

class TestPolicies(unittest.TestCase):
    def setUp(self):
        self.manager = ApprovalPolicyManager()

    def test_get_policy(self):
        self.assertEqual(self.manager.get_policy(RunMode.SAFE), SAFE_POLICY)
        self.assertEqual(self.manager.get_policy(RunMode.YOLO_WORKTREE), YOLO_WORKTREE_POLICY)
        self.assertEqual(self.manager.get_policy(RunMode.REVIEW_ONLY), REVIEW_ONLY_POLICY)
        # Default to SAFE
        self.assertEqual(self.manager.get_policy("unknown"), SAFE_POLICY)

    def test_evaluate_request_safe(self):
        policy = SAFE_POLICY
        # Safe policy usually requires user approval
        approved, reason = self.manager.evaluate_request(policy, {"title": "Write file"})
        self.assertIsNone(approved)
        self.assertIn("user approval", reason)

    def test_evaluate_request_yolo(self):
        policy = YOLO_WORKTREE_POLICY
        # YOLO auto-approves most things
        approved, reason = self.manager.evaluate_request(policy, {"title": "Write file"})
        self.assertTrue(approved)
        self.assertIn("auto-approves", reason)

        # YOLO forbids git push
        approved, reason = self.manager.evaluate_request(policy, {"title": "git push origin main"})
        self.assertFalse(approved)
        self.assertIn("forbids git push", reason)

        # YOLO forbids access to forbidden paths
        approved, reason = self.manager.evaluate_request(policy, {"title": "read .env"})
        self.assertFalse(approved)
        self.assertIn("forbids access to .env", reason)

    def test_evaluate_request_review_only(self):
        policy = REVIEW_ONLY_POLICY
        # Review only forbids writes
        approved, reason = self.manager.evaluate_request(policy, {"title": "Write file"})
        self.assertFalse(approved)
        self.assertIn("forbids write", reason)

        # Review only forbids exec
        approved, reason = self.manager.evaluate_request(policy, {"title": "Run command"})
        self.assertFalse(approved)
        self.assertIn("forbids execution", reason)

if __name__ == "__main__":
    unittest.main()
