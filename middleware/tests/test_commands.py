import asyncio
import unittest
from cardputer_codex_terminal.config import AppConfig
from cardputer_codex_terminal.core import MiddlewareApp
from cardputer_codex_terminal.runs import RunMode
from cardputer_codex_terminal.events import EventType

class CommandTests(unittest.TestCase):
    def test_mode_command(self):
        async def scenario():
            app = MiddlewareApp(
                AppConfig(
                    use_mock_codex=True,
                    branch="feature/test",
                )
            )
            await app.initialize()
            
            # Initial mode should be SAFE
            run = app.run_index.current()
            self.assertEqual(run.mode, RunMode.SAFE)
            self.assertEqual(run.badge_mode, "SAFE")
            
            # Change to YOLO
            events = await app.handle_text_prompt("/mode yolo")
            if not events:
                self.fail("No events returned from /mode yolo")
            self.assertEqual(events[0].payload.get("kind"), "mode_changed", f"Expected mode_changed, got {events[0].payload}")
            self.assertEqual(run.mode, RunMode.YOLO_WORKTREE)
            self.assertEqual(run.badge_mode, "YOLO-WORKTREE")
            
            # Change to REVIEW
            events = await app.handle_text_prompt("/mode review")
            self.assertEqual(run.mode, RunMode.REVIEW_ONLY)
            self.assertEqual(run.badge_mode, "REVIEW-ONLY")
            
            # Change back to SAFE
            events = await app.handle_text_prompt("/mode safe")
            self.assertEqual(run.mode, RunMode.SAFE)
            self.assertEqual(run.badge_mode, "SAFE")
            
            return True

        asyncio.run(scenario())

    def test_yolo_on_protected_branch_is_blocked(self):
        async def scenario():
            app = MiddlewareApp(
                AppConfig(
                    use_mock_codex=True,
                )
            )
            await app.initialize()
            
            run = app.run_index.current()
            run.base_branch = "main" # Protected
            
            # Try to change to YOLO
            events = await app.handle_text_prompt("/mode yolo")
            self.assertEqual(events[0].type, EventType.ERROR)
            self.assertIn("Cannot enable YOLO on protected branch", events[0].payload["content"])
            self.assertEqual(run.mode, RunMode.SAFE)
            
            # Change to non-protected branch
            run.base_branch = "feature/test"
            events = await app.handle_text_prompt("/mode yolo")
            self.assertEqual(events[0].payload["kind"], "mode_changed")
            self.assertEqual(run.mode, RunMode.YOLO_WORKTREE)
            
            return True

        asyncio.run(scenario())

if __name__ == "__main__":
    unittest.main()
