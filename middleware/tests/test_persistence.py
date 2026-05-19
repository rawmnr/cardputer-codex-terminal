from __future__ import annotations

import asyncio
import tempfile
import unittest
from pathlib import Path

from cardputer_codex_terminal.config import AppConfig
from cardputer_codex_terminal.core import MiddlewareApp
from cardputer_codex_terminal.runs import RunStatus


class PersistenceTests(unittest.TestCase):
    def test_restart_loads_runs_and_redacts_persisted_text(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            workspace = root / "workspace"
            workspace.mkdir()
            state_path = root / "runs.json"

            app1 = MiddlewareApp(
                AppConfig(
                    use_mock_codex=True,
                    workspace_path=str(workspace),
                    state_path=str(state_path),
                )
            )
            run = app1.run_index.current()
            self.assertIsNotNone(run)
            assert run is not None
            run.status = RunStatus.RUNNING
            run.thread_id = "thread-123"
            run.worktree_path = str(root / "missing-worktree")
            run.current_step = "super secret prompt"
            run.last_event = "super secret output"
            app1.persistence.save(app1.run_index)

            raw = state_path.read_text(encoding="utf-8")
            self.assertNotIn("super secret prompt", raw)
            self.assertNotIn("super secret output", raw)

            app2 = MiddlewareApp(
                AppConfig(
                    use_mock_codex=True,
                    workspace_path=str(workspace),
                    state_path=str(state_path),
                )
            )
            loaded = app2.run_index.current()
            self.assertIsNotNone(loaded)
            assert loaded is not None
            self.assertEqual(loaded.run_id, run.run_id)
            self.assertEqual(loaded.status, RunStatus.PAUSED)
            self.assertFalse(loaded.worktree_exists)
            self.assertIn("worktree missing", loaded.staleness_reason)
            self.assertEqual(app2.session.run_id, loaded.run_id)

    def test_cleanup_removes_orphan_runs(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            workspace = root / "workspace"
            workspace.mkdir()
            state_path = root / "runs.json"

            app = MiddlewareApp(
                AppConfig(
                    use_mock_codex=True,
                    workspace_path=str(workspace),
                    state_path=str(state_path),
                )
            )
            run = app.run_index.current()
            self.assertIsNotNone(run)
            assert run is not None
            run.status = RunStatus.PAUSED
            run.worktree_path = str(root / "missing-worktree")
            run.worktree_exists = False
            run.staleness_reason = "worktree missing"
            app.persistence.save(app.run_index)

            async def scenario() -> list[dict[str, object]]:
                out = await app.handle_text_prompt("/runs cleanup")
                return [event.to_dict() for event in out]

            payloads = asyncio.run(scenario())
            self.assertEqual(payloads[0]["payload"]["kind"], "runs_cleanup")
            self.assertEqual(payloads[0]["payload"]["removed"], 1)
            self.assertEqual(app.run_index.runs, {})


if __name__ == "__main__":
    unittest.main()
