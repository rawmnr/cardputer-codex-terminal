import unittest

from cardputer_codex_terminal.projections import CardputerProjection
from cardputer_codex_terminal.runs import ApprovalRequest, DiffSummary, RunIndex, RunMode, RunRole, RunStatus, TestSummary as RunTestSummary
from cardputer_codex_terminal.session import SessionIndex, SessionState


class TestProjections(unittest.TestCase):
    def setUp(self):
        self.session_index = SessionIndex()
        self.run_index = RunIndex()
        self.projection = CardputerProjection(self.session_index, self.run_index)
        self.session = SessionState(session_id="session-001")

    def test_build_run_list(self):
        self.run_index.create_run(role=RunRole.WORKER, mode=RunMode.SAFE, status=RunStatus.RUNNING)
        self.run_index.create_run(role=RunRole.TESTER, mode=RunMode.YOLO_WORKTREE, status=RunStatus.APPROVAL)

        proj = self.projection.build_run_list()
        self.assertEqual(proj["type"], "run_list")
        self.assertEqual(proj["v"], self.projection.version)
        self.assertEqual(len(proj["payload"]["runs"]), 2)

        runs = proj["payload"]["runs"]
        self.assertEqual(runs[0]["id"], "run-000002")
        self.assertEqual(runs[0]["mode"], "yolo_worktree")
        self.assertEqual(runs[0]["status"], "approval")

        self.assertEqual(runs[1]["id"], "run-000001")
        self.assertEqual(runs[1]["mode"], "safe")

    def test_build_run_detail_without_approval(self):
        run = self.run_index.create_run(
            role=RunRole.WORKER,
            mode=RunMode.SAFE,
            status=RunStatus.RUNNING,
            current_step="Implementing feature X",
            last_event="File written",
        )
        run.thread_id = "thread-123"
        run.diff_summary = DiffSummary(files_changed=2, insertions=10, deletions=5, summary="feat: X")
        run.test_summary = RunTestSummary(tests_run=5, passed=4, failed=1, summary="5 tests, 1 fail")
        run.danger_summary = "SAFE"
        run.badge_mode = "SAFE"
        run.merge_ready = True
        run.worktree_path = "/tmp/worktrees/demo"

        proj = self.projection.build_run_detail(run.run_id)
        self.assertEqual(proj["type"], "run_detail")
        self.assertEqual(proj["v"], self.projection.version)
        payload = proj["payload"]
        self.assertEqual(payload["id"], run.run_id)
        self.assertEqual(payload["step"], "Implementing feature X")
        self.assertIsNone(payload["approval"])
        self.assertTrue(payload["merge_ready"])
        self.assertIn({"id": "pause_run", "label": "Pause"}, payload["actions"])
        self.assertIn({"id": "stop", "label": "Stop"}, payload["actions"])
        self.assertIn({"id": "collect_diff", "label": "Diff"}, payload["actions"])
        self.assertIn({"id": "run_tests", "label": "Tests"}, payload["actions"])
        self.assertIn({"id": "generate_merge_report", "label": "Report"}, payload["actions"])
        self.assertIn({"id": "open_voice_reply", "label": "Voice reply"}, payload["actions"])
        self.assertEqual(payload["diff"]["files"], 2)
        self.assertEqual(payload["test"]["run"], 5)
        self.assertEqual(payload["danger"], "SAFE")

    def test_build_run_detail_with_approval(self):
        run = self.run_index.create_run(role=RunRole.WORKER, mode=RunMode.SAFE, status=RunStatus.APPROVAL)
        run.pending_approval = ApprovalRequest(approval_id="app-7", title="Allow write?", detail="danger", danger=True)

        proj = self.projection.build_run_detail(run.run_id)
        payload = proj["payload"]
        self.assertEqual(proj["v"], self.projection.version)
        self.assertEqual(payload["approval"]["id"], "app-7")
        self.assertIn({"id": "approve_once", "label": "Approve once"}, payload["actions"])
        self.assertIn({"id": "reject", "label": "Reject"}, payload["actions"])
        self.assertIn({"id": "stop", "label": "Stop"}, payload["actions"])

    def test_build_approval_inbox(self):
        run1 = self.run_index.create_run(role=RunRole.WORKER)
        run1.pending_approval = ApprovalRequest(approval_id="app-1", title="Allow write?", detail="detail", danger=True)
        run1.current_step = "BLE"

        run2 = self.run_index.create_run(role=RunRole.TESTER)
        run2.pending_approval = ApprovalRequest(approval_id="app-2", title="Read only?", detail="detail", danger=False)

        proj = self.projection.build_approval_inbox()
        approvals = proj["payload"]["approvals"]
        self.assertEqual(proj["v"], self.projection.version)
        self.assertEqual(len(approvals), 2)
        self.assertEqual(approvals[0]["id"], "app-1")
        self.assertEqual(approvals[0]["danger_level"], "high")
        self.assertEqual(approvals[1]["id"], "app-2")

    def test_build_status_snapshot(self):
        run = self.run_index.create_run(role=RunRole.MAIN)
        self.session.run_id = run.run_id
        self.session.title = "My Session"
        self.session.codex_usage_percent = 42

        proj = self.projection.build_status_snapshot(self.session)
        self.assertEqual(proj["type"], "status_snapshot")
        self.assertEqual(proj["v"], self.projection.version)
        self.assertEqual(proj["payload"]["title"], "My Session")
        self.assertEqual(proj["payload"]["usage"], 42)
        self.assertEqual(proj["payload"]["run"]["id"], run.run_id)

    def test_build_session_status(self):
        session = self.session_index.ensure_active(workspace_path="C:/repo", branch="feature/cardputer", thread_id="thr-123", title="My Session")
        session.last_event = "File written"
        session.codex_usage_percent = 42
        run = self.run_index.create_run(role=RunRole.WORKER, mode=RunMode.SAFE, status=RunStatus.RUNNING)
        run.current_step = "Implementing feature X"
        run.branch = "feature/x"
        run.last_event = "File written"
        run.thread_id = "thread-123"
        run.worktree_path = "/tmp/wt"
        run.pending_approval = ApprovalRequest(approval_id="app-7", title="Allow write?", detail="danger", danger=True)

        proj = self.projection.build_session_status(session)
        self.assertEqual(proj["type"], "session_status")
        self.assertEqual(proj["v"], self.projection.version)
        self.assertEqual(proj["payload"]["active_session_id"], session.session_id)
        self.assertEqual(proj["payload"]["active_run_id"], run.run_id)
        self.assertEqual(proj["payload"]["sessions"][0]["session_id"], session.session_id)
        self.assertEqual(proj["payload"]["runs"][0]["run_id"], run.run_id)


if __name__ == "__main__":
    unittest.main()
