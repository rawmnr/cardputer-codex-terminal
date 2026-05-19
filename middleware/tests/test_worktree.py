import unittest
from unittest.mock import patch, MagicMock
from pathlib import Path
from cardputer_codex_terminal.worktree import WorktreeManager, WorktreeInfo, WorktreeError

class TestWorktreeManager(unittest.TestCase):
    def setUp(self):
        self.repo_path = Path("/fake/repo")
        self.manager = WorktreeManager(self.repo_path)

    def test_is_protected(self):
        self.assertTrue(self.manager.is_protected("main"))
        self.assertTrue(self.manager.is_protected("prod"))
        self.assertFalse(self.manager.is_protected("feature/test"))

    @patch("subprocess.run")
    def test_list_worktrees_parsing(self, mock_run):
        mock_run.return_value = MagicMock(stdout="worktree /fake/repo/wt1\ncommit 123\nbranch main\n\nworktree /fake/repo/wt2\ncommit 456\nbranch feat/x\n", check=True)
        
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

if __name__ == "__main__":
    unittest.main()
