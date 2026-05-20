from __future__ import annotations

import asyncio
from unittest import mock
import unittest

from cardputer_codex_terminal.config import AppConfig
from cardputer_codex_terminal.core import MiddlewareApp
from cardputer_codex_terminal.mcp import CardputerMcpServer
from cardputer_codex_terminal.messages import CardputerMessage, CardputerMessageType


class McpServerTests(unittest.TestCase):
    def test_initialize_and_tool_list_expose_cardputer_tools(self) -> None:
        async def scenario() -> tuple[dict[str, object], dict[str, object]]:
            app = MiddlewareApp(AppConfig(use_mock_codex=True))
            await app.initialize()
            server = CardputerMcpServer(app)

            init = await server.handle_message(
                {
                    "jsonrpc": "2.0",
                    "id": 1,
                    "method": "initialize",
                    "params": {
                        "protocolVersion": "2025-06-18",
                        "capabilities": {},
                        "clientInfo": {"name": "test", "version": "1"},
                    },
                }
            )
            await server.handle_message({"jsonrpc": "2.0", "method": "notifications/initialized"})
            tools = await server.handle_message({"jsonrpc": "2.0", "id": 2, "method": "tools/list", "params": {}})
            return init or {}, tools or {}

        init, tools = asyncio.run(scenario())

        self.assertEqual(init["result"]["protocolVersion"], "2025-06-18")
        tool_names = [tool["name"] for tool in tools["result"]["tools"]]
        self.assertEqual(
            tool_names,
            [
                "cardputer.notify",
                "cardputer.ask",
                "cardputer.confirm",
                "cardputer.show",
                "cardputer.dictate",
                "cardputer.preview_lvgl_ui",
            ],
        )
    def test_preview_lvgl_ui_returns_multiple_frames(self) -> None:
        async def scenario() -> dict[str, object]:
            app = MiddlewareApp(AppConfig(use_mock_codex=True))
            await app.initialize()
            server = CardputerMcpServer(app)
            await server.handle_message(
                {
                    "jsonrpc": "2.0",
                    "id": 1,
                    "method": "initialize",
                    "params": {
                        "protocolVersion": "2025-06-18",
                        "capabilities": {},
                        "clientInfo": {"name": "test", "version": "1"},
                    },
                }
            )
            await server.handle_message({"jsonrpc": "2.0", "method": "notifications/initialized"})

            with mock.patch(
                "cardputer_codex_terminal.mcp.run_lvgl_preview",
                return_value={
                    "image_b64": "frame-2",
                    "frame_count": 3,
                    "frames": [
                        {"index": 0, "label": "initial", "filename": "preview_0_initial.png", "width": 240, "height": 135, "image_b64": "frame-0"},
                        {"index": 1, "label": "down", "filename": "preview_1_down.png", "width": 240, "height": 135, "image_b64": "frame-1"},
                        {"index": 2, "label": "enter", "filename": "preview_2_enter.png", "width": 240, "height": 135, "image_b64": "frame-2"},
                    ],
                    "width": 240,
                    "height": 135,
                    "fixture": {"active_app": 0, "actions": ["down", "enter"]},
                    "actions": ["down", "enter"],
                    "stdout": "ok",
                    "stderr": "",
                },
            ):
                return await server.handle_message(
                    {
                        "jsonrpc": "2.0",
                        "id": 2,
                        "method": "tools/call",
                        "params": {
                            "name": "cardputer.preview_lvgl_ui",
                            "arguments": {"screen": "buddy", "actions": ["down", "enter"]},
                        },
                    }
                ) or {}

        response = asyncio.run(scenario())

        self.assertEqual(response["result"]["structuredContent"]["frame_count"], 3)
        self.assertEqual(
            [frame["label"] for frame in response["result"]["structuredContent"]["frames"]],
            ["initial", "down", "enter"],
        )
        self.assertEqual(
            [content["type"] for content in response["result"]["content"]],
            ["text", "image", "image", "image"],
        )

    def test_notify_and_show_update_prompt_state(self) -> None:
        async def scenario() -> tuple[dict[str, object], dict[str, object], dict[str, object]]:
            app = MiddlewareApp(AppConfig(use_mock_codex=True))
            await app.initialize()
            server = CardputerMcpServer(app)
            await server.handle_message(
                {
                    "jsonrpc": "2.0",
                    "id": 1,
                    "method": "initialize",
                    "params": {
                        "protocolVersion": "2025-06-18",
                        "capabilities": {},
                        "clientInfo": {"name": "test", "version": "1"},
                    },
                }
            )
            await server.handle_message({"jsonrpc": "2.0", "method": "notifications/initialized"})

            notify = await server.handle_message(
                {
                    "jsonrpc": "2.0",
                    "id": 2,
                    "method": "tools/call",
                    "params": {
                        "name": "cardputer.notify",
                        "arguments": {"title": "Build ready", "body": "Binary staged", "urgency": "high"},
                    },
                }
            )
            show = await server.handle_message(
                {
                    "jsonrpc": "2.0",
                    "id": 3,
                    "method": "tools/call",
                    "params": {
                        "name": "cardputer.show",
                        "arguments": {"text": "SYNCING", "channel": "status"},
                    },
                }
            )
            return notify or {}, show or {}, {
                "kind": app.session.bridge_prompt_kind,
                "title": app.session.bridge_prompt_title,
                "detail": app.session.bridge_prompt_detail,
                "channel": app.session.bridge_prompt_channel,
                "urgency": app.session.bridge_prompt_urgency,
            }

        notify, show, session = asyncio.run(scenario())

        self.assertEqual(notify["result"]["structuredContent"], {"status": "delivered", "title": "Build ready", "body": "Binary staged", "urgency": "high"})
        self.assertEqual(show["result"]["structuredContent"], {"status": "displayed", "text": "SYNCING", "channel": "status"})
        self.assertEqual(session, {"kind": "bridge_notification", "title": "status", "detail": "SYNCING", "channel": "status", "urgency": "normal"})

    def test_ask_waits_for_cardputer_response(self) -> None:
        async def scenario() -> tuple[dict[str, object], dict[str, object]]:
            app = MiddlewareApp(AppConfig(use_mock_codex=True))
            await app.initialize()
            server = CardputerMcpServer(app)
            await server.handle_message(
                {
                    "jsonrpc": "2.0",
                    "id": 1,
                    "method": "initialize",
                    "params": {
                        "protocolVersion": "2025-06-18",
                        "capabilities": {},
                        "clientInfo": {"name": "test", "version": "1"},
                    },
                }
            )
            await server.handle_message({"jsonrpc": "2.0", "method": "notifications/initialized"})

            task = asyncio.create_task(
                server.handle_message(
                    {
                        "jsonrpc": "2.0",
                        "id": 2,
                        "method": "tools/call",
                        "params": {
                            "name": "cardputer.ask",
                            "arguments": {
                                "question": "Pick a branch",
                                "choices": ["main", "feature"],
                                "timeout_s": 5,
                            },
                        },
                    }
                )
            )
            await asyncio.sleep(0)
            await app.handle_cardputer_message(
                CardputerMessage(
                    CardputerMessageType.BRIDGE_RESPONSE,
                    {"accepted": True, "selected_index": 1, "note": "feature"},
                )
            )
            response = await task
            return response or {}, {
                "kind": app.session.bridge_prompt_kind,
                "title": app.session.bridge_prompt_title,
                "detail": app.session.bridge_prompt_detail,
                "channel": app.session.bridge_prompt_channel,
            }

        response, session = asyncio.run(scenario())

        structured = response["result"]["structuredContent"]
        self.assertEqual(structured["status"], "answered")
        self.assertEqual(structured["selected_index"], 1)
        self.assertEqual(structured["selected_choice"], "feature")
        self.assertEqual(structured["choices"], ["main", "feature"])
        self.assertEqual(session, {"kind": None, "title": None, "detail": None, "channel": None})

