from __future__ import annotations

import asyncio
import base64
import json
import unittest
from dataclasses import asdict

from cardputer_codex_terminal.config import AppConfig
from cardputer_codex_terminal.core import MiddlewareApp
from cardputer_codex_terminal.codex_transport import LocalWebSocketCodexTransport, StdioCodexAppServerTransport
from cardputer_codex_terminal.events import EventType
from cardputer_codex_terminal.messages import CardputerMessage, CardputerMessageType
from cardputer_codex_terminal.server import CardputerBridgeServer


class MiddlewareAppTests(unittest.TestCase):
    def test_stdio_transport_is_the_real_codex_default(self) -> None:
        app = MiddlewareApp(AppConfig(use_mock_codex=False))

        self.assertIsInstance(app.transport, StdioCodexAppServerTransport)

    def test_mock_transport_initialization_and_prompt_flow(self) -> None:
        async def scenario() -> list[dict]:
            app = MiddlewareApp(
                AppConfig(
                    host="127.0.0.1",
                    port=8765,
                    codex_ws_url="ws://127.0.0.1:9000",
                    use_mock_codex=True,
                )
            )
            await app.initialize()
            events = await app.handle_text_prompt("Hello Codex")
            return [event.to_dict() for event in events]

        payloads = asyncio.run(scenario())

        self.assertEqual(
            payloads,
            [
                {
                    "type": EventType.CODEX_STATUS.value,
                    "payload": {"content": "mock_codex_ready", "kind": "status", "data": {}},
                },
                {
                    "type": EventType.CODEX_DELTA.value,
                    "payload": {"content": "Received: Hello Codex", "kind": "delta", "data": {}},
                },
                {
                    "type": EventType.CODEX_USAGE.value,
                    "payload": {
                        "content": "mock_usage_update",
                        "kind": "usage",
                        "data": {"threadId": "mock-thread:.:main", "cwd": ".", "usedPercent": 12},
                        "codex_usage_percent": 12,
                        "codex_usage_secondary_percent": -1,
                        "codex_usage_window_minutes": 0,
                        "codex_usage_secondary_window_minutes": 0,
                        "codex_usage_resets_at": 0,
                        "codex_usage_secondary_resets_at": 0,
                        "codex_usage_reset_line": "Reset data pending",
                    },
                },
                {
                    "type": EventType.CODEX_DELTA.value,
                    "payload": {"content": "This transport is a local placeholder.", "kind": "delta", "data": {}},
                },
                {
                    "type": EventType.CODEX_STATUS.value,
                    "payload": {"content": "mock_turn_completed", "kind": "completed", "data": {}},
                },
            ],
        )

    def test_status_request_includes_session_snapshot(self) -> None:
        async def scenario() -> dict[str, object]:
            app = MiddlewareApp(
                AppConfig(
                    host="127.0.0.1",
                    port=8765,
                    codex_ws_url="ws://127.0.0.1:9000",
                    use_mock_codex=True,
                )
            )
            await app.initialize()
            await app.handle_text_prompt("Hello Codex")
            events = await app.handle_cardputer_message(CardputerMessage(CardputerMessageType.STATUS_REQUEST, {}))
            payload = events[0].to_dict()["payload"]
            return payload

        payload = asyncio.run(scenario())

        self.assertEqual(payload["active_session_id"], "session-000001")
        self.assertEqual(len(payload["sessions"]), 1)
        active_session = payload["sessions"][0]
        self.assertEqual(active_session["title"], "Hello Codex")
        self.assertEqual(active_session["status"], "done")
        self.assertGreaterEqual(len(active_session["events"]), 1)
        self.assertEqual(active_session["session_id"], "session-000001")

    def test_voice_prompt_buffer_transcribes_to_prompt_flow(self) -> None:
        async def scenario() -> tuple[list[dict], dict[str, object]]:
            app = MiddlewareApp(
                AppConfig(
                    host="127.0.0.1",
                    port=8765,
                    codex_ws_url="ws://127.0.0.1:9000",
                    use_mock_codex=True,
                )
            )
            await app.initialize()
            chunk = base64.b64encode(b"\x01\x00\x02\x00\x03\x00\x04\x00").decode("ascii")
            chunk_event = await app.handle_cardputer_message(
                CardputerMessage(CardputerMessageType.AUDIO_CHUNK, {"chunk_id": 1, "pcm_b64": chunk})
            )
            voice_events = await app.handle_cardputer_message(
                CardputerMessage(CardputerMessageType.VOICE_PROMPT_READY, {"sample_rate_hz": 16000})
            )
            return (
                [event.to_dict() for event in chunk_event + voice_events],
                {
                    "buffered_chunks": app.voice_buffer.chunk_count,
                    "buffered_samples": app.voice_buffer.sample_count(),
                },
            )

        events, state = asyncio.run(scenario())

        self.assertEqual(
            events[0],
            {
                "type": EventType.AUDIO_CHUNK.value,
                "payload": {"chunk_id": 1, "sample_count": 4, "chunk_count": 1, "byte_count": 8},
            },
        )
        self.assertEqual(
            events[1],
            {
                "type": EventType.CODEX_STATUS.value,
                "payload": {
                    "kind": "voice_prompt_transcribed",
                    "content": "Voice prompt captured (4 samples, 0 ms at 16000 Hz).",
                },
            },
        )
        self.assertEqual(events[2]["type"], EventType.CODEX_STATUS.value)
        self.assertEqual(events[-1]["type"], EventType.CODEX_STATUS.value)
        self.assertEqual(state, {"buffered_chunks": 0, "buffered_samples": 0})

    def test_project_and_branch_selection_update_session_and_thread(self) -> None:
        class FakeWebSocket:
            def __init__(self) -> None:
                self.sent: list[str] = []
                self.messages = [
                    json.dumps({"id": "initialize-1", "result": {}}),
                    json.dumps({"id": "thread/start-2", "result": {"thread": {"id": "thr_123"}}}),
                    json.dumps({"id": "thread/metadata/update-3", "result": {}}),
                    json.dumps({"id": "turn-start-4", "result": {"turn": {"id": "turn_123", "items": [], "status": "inProgress"}}}),
                    json.dumps(
                        {
                            "method": "turn/completed",
                            "params": {"threadId": "thr_123", "turn": {"id": "turn_123", "items": [], "status": "completed"}},
                        }
                    ),
                ]

            async def send(self, message: str) -> None:
                self.sent.append(message)

            async def recv(self) -> str:
                return self.messages.pop(0)

        async def connect_factory(_: str) -> FakeWebSocket:
            return FakeWebSocket()

        async def scenario() -> tuple[list[str], dict[str, object]]:
            app = MiddlewareApp(
                AppConfig(
                    host="127.0.0.1",
                    port=8765,
                    codex_ws_url="ws://127.0.0.1:9000",
                    codex_transport="websocket",
                    use_mock_codex=False,
                )
            )
            assert isinstance(app.transport, LocalWebSocketCodexTransport)
            app.transport = LocalWebSocketCodexTransport("ws://127.0.0.1:9000", connect_factory=connect_factory)

            await app.initialize()
            await app.handle_cardputer_message(
                CardputerMessage(CardputerMessageType.PROJECT_SELECT, {"workspace_path": "C:/repo"})
            )
            await app.handle_cardputer_message(
                CardputerMessage(CardputerMessageType.BRANCH_SELECT, {"branch": "feature/cardputer"})
            )
            events = await app.handle_cardputer_message(
                CardputerMessage(CardputerMessageType.TEXT_PROMPT, {"text": "Hello Codex"})
            )
            websocket = app.transport._ws
            assert websocket is not None
            return websocket.sent, {
                "session": {
                    "workspace_path": app.session.workspace_path,
                    "branch": app.session.branch,
                    "thread_id": app.session.thread_id,
                },
                "events": [event.to_dict() for event in events],
            }

        sent, state = asyncio.run(scenario())

        self.assertEqual(state["session"], {"workspace_path": "C:/repo", "branch": "feature/cardputer", "thread_id": "thr_123"})
        self.assertTrue(any('"method": "thread/start"' in item for item in sent))
        self.assertTrue(any('"method": "thread/metadata/update"' in item for item in sent))
        self.assertTrue(any('"method": "turn/start"' in item for item in sent))
        self.assertEqual(
            state["events"],
            [
                {
                    "type": EventType.CODEX_STATUS.value,
                    "payload": {
                        "content": "turn_completed",
                        "kind": "completed",
                        "data": {"threadId": "thr_123", "turn": {"id": "turn_123", "items": [], "status": "completed"}},
                    },
                },
                {
                    "type": EventType.CODEX_STATUS.value,
                    "payload": {
                        "kind": "session_status",
                        "content": "idle",
                        "threadId": "thr_123",
                        "status": "done",
                    },
                },
            ],
        )

    def test_bridge_prompt_flow_updates_session(self) -> None:
        async def scenario() -> tuple[list[dict], list[dict], dict[str, object]]:
            app = MiddlewareApp(
                AppConfig(
                    host="127.0.0.1",
                    port=8765,
                    codex_ws_url="ws://127.0.0.1:9000",
                    use_mock_codex=True,
                )
            )
            await app.initialize()
            notification_events = await app.handle_cardputer_message(
                CardputerMessage(
                    CardputerMessageType.BRIDGE_NOTIFICATION,
                    {"title": "Build ready", "detail": "Firmware binary is ready."},
                )
            )
            question_events = await app.handle_cardputer_message(
                CardputerMessage(
                    CardputerMessageType.BRIDGE_QUESTION,
                    {"title": "Select workspace", "detail": "Choose one", "options": ["repo-a", "repo-b"]},
                )
            )
            response_events = await app.handle_cardputer_message(
                CardputerMessage(
                    CardputerMessageType.BRIDGE_RESPONSE,
                    {"accepted": True, "selected_index": 1, "note": "Use repo-b"},
                )
            )
            return (
                [event.to_dict() for event in notification_events],
                [event.to_dict() for event in question_events + response_events],
                {
                    "bridge_prompt_kind": app.session.bridge_prompt_kind,
                    "bridge_prompt_title": app.session.bridge_prompt_title,
                    "bridge_prompt_options": list(app.session.bridge_prompt_options),
                },
            )

        notification_events, response_events, session = asyncio.run(scenario())

        self.assertEqual(notification_events[0]["payload"]["kind"], "bridge_notification")
        self.assertEqual(notification_events[0]["payload"]["title"], "Build ready")
        self.assertEqual(response_events[0]["payload"]["kind"], "bridge_question")
        self.assertEqual(response_events[1]["payload"]["kind"], "bridge_response")
        self.assertEqual(session, {"bridge_prompt_kind": None, "bridge_prompt_title": None, "bridge_prompt_options": []})

    def test_bridge_connection_reset_is_treated_as_disconnect(self) -> None:
        class FakeWebSocket:
            def __init__(self) -> None:
                self.sent: list[str] = []

            async def send(self, message: str) -> None:
                self.sent.append(message)

            def __aiter__(self) -> "FakeWebSocket":
                return self

            async def __anext__(self) -> str:
                raise ConnectionResetError("network connection dropped")

        sentinel = object()

        async def scenario() -> tuple[list[str], object | None]:
            app = MiddlewareApp(
                AppConfig(
                    host="127.0.0.1",
                    port=8765,
                    codex_ws_url="ws://127.0.0.1:9000",
                    use_mock_codex=True,
                )
            )
            await app.initialize()
            app.event_observer = sentinel
            bridge = CardputerBridgeServer(app)
            websocket = FakeWebSocket()
            await bridge.handle_connection(websocket)
            return websocket.sent, app.event_observer

        sent, observer = asyncio.run(scenario())

        self.assertTrue(sent)
        self.assertIs(observer, sentinel)

    def test_display_snapshot_updates_preview_event_channel(self) -> None:
        async def scenario() -> list[dict]:
            app = MiddlewareApp(
                AppConfig(
                    host="127.0.0.1",
                    port=8765,
                    codex_ws_url="ws://127.0.0.1:9000",
                    use_mock_codex=True,
                )
            )
            await app.initialize()
            events = await app.handle_cardputer_message(
                CardputerMessage(
                    CardputerMessageType.DISPLAY_SNAPSHOT,
                    {"screen_text": "LIVE SCREEN", "status_line": "firmware snapshot", "active_app": "Codex Buddy"},
                )
            )
            return [event.to_dict() for event in events]

        payloads = asyncio.run(scenario())

        self.assertEqual(
            payloads,
            [
                {
                    "type": "display_snapshot",
                    "payload": {
                        "kind": "display_snapshot",
                        "content": "firmware snapshot",
                        "screen_text": "LIVE SCREEN",
                        "active_app": "Codex Buddy",
                        "status_line": "firmware snapshot",
                    },
                }
            ],
        )

    def test_approval_request_and_response_flow_updates_session_and_transport(self) -> None:
        class FakeWebSocket:
            def __init__(self) -> None:
                self.sent: list[str] = []
                self.messages = [
                    json.dumps({"id": "initialize-1", "result": {}}),
                    json.dumps({"id": "thread/start-2", "result": {"thread": {"id": "thr_456"}}}),
                    json.dumps({"id": "turn-start-3", "result": {"turn": {"id": "turn_456", "items": [], "status": "inProgress"}}}),
                    json.dumps(
                        {
                            "id": "server-appr-123",
                            "method": "item/commandExecution/requestApproval",
                            "params": {
                                "approvalId": "appr_123",
                                "command": "Remove-Item build",
                                "itemId": "item_123",
                                "reason": "Codex needs approval before removing build artifacts.",
                                "threadId": "thr_456",
                                "turnId": "turn_456",
                            },
                        }
                    ),
                ]

            async def send(self, message: str) -> None:
                self.sent.append(message)

            async def recv(self) -> str:
                return self.messages.pop(0)

        async def connect_factory(_: str) -> FakeWebSocket:
            return FakeWebSocket()

        async def scenario() -> tuple[list[dict], list[dict], dict[str, object]]:
            app = MiddlewareApp(
                AppConfig(
                    host="127.0.0.1",
                    port=8765,
                    codex_ws_url="ws://127.0.0.1:9000",
                    codex_transport="websocket",
                    use_mock_codex=False,
                )
            )
            assert isinstance(app.transport, LocalWebSocketCodexTransport)
            app.transport = LocalWebSocketCodexTransport("ws://127.0.0.1:9000", connect_factory=connect_factory)

            await app.initialize()
            prompt_events = await app.handle_text_prompt("Please remove the build artifacts")
            response_events = await app.handle_cardputer_message(
                CardputerMessage(
                    CardputerMessageType.APPROVAL_RESPONSE,
                    {"approved": True, "approval_id": "server-appr-123", "note": "Looks good to me."},
                )
            )
            websocket = app.transport._ws
            assert websocket is not None
            return (
                [event.to_dict() for event in prompt_events],
                [event.to_dict() for event in response_events],
                {
                    "session": {
                        "workspace_path": app.session.workspace_path,
                        "branch": app.session.branch,
                        "thread_id": app.session.thread_id,
                        "pending_approval_id": app.session.pending_approval_id,
                    },
                    "sent": websocket.sent,
                },
            )

        prompt_events, response_events, state = asyncio.run(scenario())

        self.assertEqual(
            prompt_events,
            [
                {
                    "type": EventType.APPROVAL_REQUEST.value,
                    "payload": {
                        "approval_id": "server-appr-123",
                        "detail": "Codex needs approval before removing build artifacts.",
                        "timeout_seconds": None,
                        "title": "Remove-Item build",
                    },
                }
            ],
        )
        self.assertEqual(
            response_events,
            [
                {
                    "type": EventType.APPROVAL_RESPONSE.value,
                    "payload": {"approval_id": "server-appr-123", "approved": True, "note": "Looks good to me."},
                }
            ],
        )
        self.assertEqual(
            state["session"],
            {"workspace_path": ".", "branch": None, "thread_id": "thr_456", "pending_approval_id": None},
        )
        self.assertTrue(any('"id": "server-appr-123"' in item and '"decision": "accept"' in item for item in state["sent"]))

    def test_local_websocket_transport_reports_target_url(self) -> None:
        class FakeWebSocket:
            def __init__(self) -> None:
                self.sent: list[str] = []
                self.messages = [
                    json.dumps({"id": "initialize-1", "result": {}}),
                    json.dumps({"id": "turn-start-2", "result": {"turn": {"id": "turn_123", "items": [], "status": "inProgress"}}}),
                    json.dumps({"method": "item/agentMessage/delta", "params": {"delta": "partial"}}),
                    json.dumps(
                        {
                            "method": "turn/completed",
                            "params": {"threadId": "thr_123", "turn": {"id": "turn_123", "items": [], "status": "completed"}},
                        }
                    ),
                ]

            async def send(self, message: str) -> None:
                self.sent.append(message)

            async def recv(self) -> str:
                return self.messages.pop(0)

        async def connect_factory(_: str) -> FakeWebSocket:
            return FakeWebSocket()

        transport = LocalWebSocketCodexTransport("ws://127.0.0.1:9000", connect_factory=connect_factory)

        async def scenario() -> list[dict]:
            await transport.initialize()
            events = []
            async for reply in transport.start_turn("Hello Codex", thread_id="thr_123"):
                events.append(reply)
            return [asdict(event) for event in events]

        payloads = asyncio.run(scenario())

        self.assertEqual(
            payloads,
            [
                {"kind": "delta", "content": "partial", "data": {"delta": "partial"}},
                {
                    "kind": "completed",
                    "content": "turn_completed",
                    "data": {"threadId": "thr_123", "turn": {"id": "turn_123", "items": [], "status": "completed"}},
                },
            ],
        )

    def test_local_websocket_transport_parses_usage_notifications(self) -> None:
        class FakeWebSocket:
            def __init__(self) -> None:
                self.sent: list[str] = []
                self.messages = [
                    json.dumps({"id": "initialize-1", "result": {}}),
                    json.dumps({"id": "turn-start-2", "result": {"turn": {"id": "turn_123", "items": [], "status": "inProgress"}}}),
                    json.dumps(
                        {
                            "method": "thread/tokenUsage/updated",
                            "params": {
                                "threadId": "thr_123",
                                "turnId": "turn_123",
                                "tokenUsage": {
                                    "total": {
                                        "inputTokens": 120,
                                        "outputTokens": 42,
                                        "cachedInputTokens": 8,
                                        "reasoningOutputTokens": 16,
                                        "totalTokens": 186,
                                    },
                                    "last": {
                                        "inputTokens": 120,
                                        "outputTokens": 42,
                                        "cachedInputTokens": 8,
                                        "reasoningOutputTokens": 16,
                                        "totalTokens": 186,
                                    },
                                },
                            },
                        }
                    ),
                    json.dumps(
                        {
                            "method": "turn/completed",
                            "params": {"threadId": "thr_123", "turn": {"id": "turn_123", "items": [], "status": "completed"}},
                        }
                    ),
                ]

            async def send(self, message: str) -> None:
                self.sent.append(message)

            async def recv(self) -> str:
                return self.messages.pop(0)

        async def connect_factory(_: str) -> FakeWebSocket:
            return FakeWebSocket()

        transport = LocalWebSocketCodexTransport("ws://127.0.0.1:9000", connect_factory=connect_factory)

        async def scenario() -> list[dict]:
            await transport.initialize()
            replies = []
            async for reply in transport.start_turn("Hello Codex", thread_id="thr_123"):
                replies.append(asdict(reply))
            return replies

        replies = asyncio.run(scenario())

        self.assertEqual(
            replies,
            [
                {
                    "kind": "usage",
                    "content": "thread usage input=120 output=42 cached=8 reasoning=16",
                    "data": {
                        "method": "thread/tokenUsage/updated",
                        "threadId": "thr_123",
                        "turnId": "turn_123",
                        "tokenUsage": {
                            "total": {
                                "inputTokens": 120,
                                "outputTokens": 42,
                                "cachedInputTokens": 8,
                                "reasoningOutputTokens": 16,
                                "totalTokens": 186,
                            },
                            "last": {
                                "inputTokens": 120,
                                "outputTokens": 42,
                                "cachedInputTokens": 8,
                                "reasoningOutputTokens": 16,
                                "totalTokens": 186,
                            },
                        },
                    },
                },
                {
                    "kind": "completed",
                    "content": "turn_completed",
                    "data": {"threadId": "thr_123", "turn": {"id": "turn_123", "items": [], "status": "completed"}},
                },
            ],
        )
