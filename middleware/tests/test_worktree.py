import unittest
from pathlib import Path
from unittest.mock import MagicMock, patch

from cardputer_codex_terminal.runs import AgentRun, DiffSummary, RunMode, RunStatus, TestSummary as RunTestSummary
from cardputer_codex_terminal.worktree import WorktreeError, WorktreeInfo, WorktreeManager



class TestWorktreeManager(unittest.TestCase):
    def setUp(self):
        self.repo_path = Path("/fake/repo")
        self.manager = WorktreeManager(self.repo_path)

    def test_is_protected(self):
        self.assertTrue(self.manager.is_protected("main"))
        self.assertTrue(self.manager.is_protected("prod"))
        self.assertTrue(self.manager.is_protected("refs/heads/main"))
        self.assertFalse(self.manager.is_protected("feature/test"))

    @patch("subprocess.run")
    def test_list_worktrees_parsing(self, mock_run):
        mock_run.return_value = MagicMock(stdout="worktree /fake/repo/wt1\ncommit 123\nbranch refs/heads/main\n\nworktree /fake/repo/wt2\ncommit 456\nbranch refs/heads/feat/x\n", check=True)

        wts = self.manager.list_worktrees()
        self.assertEqual(len(wts), 2)
        self.assertEqual(wts[0].path, Path("/fake/repo/wt1"))
        self.assertEqual(wts[0].branch, "main")
        self.assertEqual(wts[1].path, Path("/fake/repo/wt2"))
        self.assertEqual(wts[1].branch, "feat/x")

    @patch("subprocess.run")
    def test_create_worktree(self, mock_run):
        self.manager.create_worktree("main", "test-run")
        mock_run.assert_called_with(
            ["git", "worktree", "add", "-b", "run-test-run", str(self.repo_path.parent / "worktrees" / "test-run"), "main"],
            cwd=self.repo_path,
            capture_output=True,
            text=True,
            check=True,
        )

    @patch("subprocess.run")
    def test_remove_worktree(self, mock_run):
        self.manager.remove_worktree(Path("/fake/repo/wt1"))
        mock_run.assert_called_with(
            ["git", "worktree", "remove", str(Path("/fake/repo/wt1"))],
            cwd=self.repo_path,
            capture_output=True,
            text=True,
            check=True,
        )

    def test_get_worktree_by_name(self):
        with patch.object(WorktreeManager, "list_worktrees") as mock_list:
            mock_list.return_value = [
                WorktreeInfo(Path("/fake/wt1"), "123", "main"),
                WorktreeInfo(Path("/fake/wt2"), "456", "feat"),
            ]
            self.assertEqual(self.manager.get_worktree_by_name("wt1"), Path("/fake/wt1"))
            self.assertEqual(self.manager.get_worktree_by_name("wt2"), Path("/fake/wt2"))
            self.assertIsNone(self.manager.get_worktree_by_name("wt3"))

    @patch("subprocess.run")
    def test_collect_diff_summarizes_shortstat_and_files(self, mock_run):
        mock_run.side_effect = [
            MagicMock(stdout="2 files changed, 2 insertions(+), 3 deletions(-)", check=True),
            MagicMock(stdout="a.py\nb.py\n", check=True),
        ]

        diff = self.manager.collect_diff(Path("/fake/repo/wt1"), "main")

        self.assertEqual(diff.files_changed, 2)
        self.assertEqual(diff.insertions, 2)
        self.assertEqual(diff.deletions, 3)
        self.assertIn("2 files changed", diff.summary)
        self.assertIn("a.py", diff.summary)
        self.assertIn("b.py", diff.summary)

    @patch("subprocess.run")
    def test_run_tests_parses_summary(self, mock_run):
        mock_run.return_value = MagicMock(
            stdout="Ran 5 tests in 0.1s\n\nFAILED (failures=1, errors=1)\n",
            stderr="",
            returncode=1,
        )

        summary = self.manager.run_tests(Path("/fake/repo/wt1"), command=["python", "-m", "unittest"])

        self.assertEqual(summary.tests_run, 5)
        self.assertEqual(summary.failed, 2)
        self.assertEqual(summary.passed, 3)
        self.assertEqual(summary.skipped, 0)

        run = AgentRun("run-000001", workspace_path="/fake/repo", worktree_path="/fake/repo/wt1", branch="feature/x", mode=RunMode.YOLO_WORKTREE, status=RunStatus.DONE)
        run.diff_summary = DiffSummary(files_changed=2, insertions=10, deletions=5, summary="feat: X")
        run.test_summary = RunTestSummary(tests_run=5, passed=4, failed=1, summary="5 tests, 1 failed")
        run.merge_ready = True

        report = self.manager.generate_merge_report(run)

        self.assertIn("Run: run-000001", report)
        self.assertIn("Branch: feature/x", report)
        self.assertIn("Merge ready: yes", report)
        self.assertIn("Diff: feat: X", report)
        self.assertIn("Tests: 5 tests, 1 failed", report)


if __name__ == "__main__":
    unittest.main()
