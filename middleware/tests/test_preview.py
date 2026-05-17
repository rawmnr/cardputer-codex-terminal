from __future__ import annotations

import asyncio
import tempfile
import unittest
from pathlib import Path

from cardputer_codex_terminal.config import AppConfig
from cardputer_codex_terminal.core import MiddlewareApp
from cardputer_codex_terminal.events import Event, EventType
from cardputer_codex_terminal.preview import DevPreviewMirror


class PreviewMirrorTests(unittest.TestCase):
    def test_recording_events_writes_live_mirror_files(self) -> None:
        async def scenario() -> dict[str, str]:
            with tempfile.TemporaryDirectory() as tmp:
                workspace = Path(tmp)
                app = MiddlewareApp(
                    AppConfig(
                        host="127.0.0.1",
                        port=8765,
                        codex_ws_url="ws://127.0.0.1:9000",
                        use_mock_codex=True,
                        workspace_path=str(workspace),
                    )
                )
                mirror = DevPreviewMirror(app=app, workspace_root=workspace)
                app.event_observer = mirror.record_events
                await app.initialize()
                await app.handle_text_prompt("Hello Codex")

                return {
                    "state": mirror.state_path.read_text(encoding="utf-8"),
                    "screen": mirror.screen_path.read_text(encoding="utf-8"),
                    "logs": mirror.log_path.read_text(encoding="utf-8"),
                    "events": mirror.events_path.read_text(encoding="utf-8"),
                }

        payloads = asyncio.run(scenario())

        self.assertIn("Cardputer Codex Dev Mirror", payloads["screen"])
        self.assertIn("mock_codex_ready", payloads["state"])
        self.assertIn("Received: Hello Codex", payloads["logs"])
        self.assertIn("mock_turn_completed", payloads["events"])

    def test_display_snapshot_overrides_rendered_screen_text(self) -> None:
        async def scenario() -> str:
            with tempfile.TemporaryDirectory() as tmp:
                workspace = Path(tmp)
                app = MiddlewareApp(
                    AppConfig(
                        host="127.0.0.1",
                        port=8765,
                        codex_ws_url="ws://127.0.0.1:9000",
                        use_mock_codex=True,
                        workspace_path=str(workspace),
                    )
                )
                mirror = DevPreviewMirror(app=app, workspace_root=workspace)
                mirror.record_events(
                    [
                        Event(
                            EventType.DISPLAY_SNAPSHOT,
                            {
                                "screen_text": "LIVE SCREEN",
                                "status_line": "firmware snapshot",
                                "active_app": "Codex Buddy",
                            },
                        )
                    ]
                )
                return mirror.screen_path.read_text(encoding="utf-8")

        screen = asyncio.run(scenario())

        self.assertEqual(screen, "LIVE SCREEN")
