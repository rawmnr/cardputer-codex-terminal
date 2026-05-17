from __future__ import annotations

import unittest

from cardputer_codex_terminal.messages import CardputerMessage, CardputerMessageType
from cardputer_codex_terminal.router import CardputerRouter


class MessageTests(unittest.TestCase):
    def test_message_round_trip(self) -> None:
        message = CardputerMessage(CardputerMessageType.TEXT_PROMPT, {"text": "hello"})
        parsed = CardputerMessage.from_dict(message.to_dict())

        self.assertEqual(parsed.type, CardputerMessageType.TEXT_PROMPT)
        self.assertEqual(parsed.payload, {"text": "hello"})
        self.assertEqual(parsed.protocol_version, 1)

    def test_message_round_trip_keeps_protocol_version(self) -> None:
        message = CardputerMessage(CardputerMessageType.TEXT_PROMPT, {"text": "hello"}, protocol_version=1)
        parsed = CardputerMessage.from_dict(message.to_dict())

        self.assertEqual(parsed.protocol_version, 1)

    def test_message_round_trip_keeps_auth_token(self) -> None:
        message = CardputerMessage(CardputerMessageType.TEXT_PROMPT, {"text": "hello"}, auth_token="secret")
        parsed = CardputerMessage.from_dict(message.to_dict())

        self.assertEqual(parsed.auth_token, "secret")

    def test_router_handles_text_prompt(self) -> None:
        router = CardputerRouter()
        message = CardputerMessage(CardputerMessageType.TEXT_PROMPT, {"text": "hello"})

        result = router.route(message)

        self.assertEqual(result.status_line, "text prompt received")
        self.assertEqual(result.outbound_events, [{"kind": "text_prompt", "text": "hello"}])

    def test_router_handles_voice_prompt_ready(self) -> None:
        router = CardputerRouter()
        message = CardputerMessage(
            CardputerMessageType.VOICE_PROMPT_READY,
            {"sample_rate_hz": 16000, "sample_count": 2400},
        )

        result = router.route(message)

        self.assertEqual(result.status_line, "voice prompt ready")
        self.assertEqual(
            result.outbound_events,
            [{"kind": "voice_prompt_ready", "sample_rate_hz": 16000, "sample_count": 2400}],
        )

    def test_router_handles_approval_response(self) -> None:
        router = CardputerRouter()
        message = CardputerMessage(
            CardputerMessageType.APPROVAL_RESPONSE,
            {"approved": True, "approval_id": "appr_123", "note": "Approved"},
        )

        result = router.route(message)

        self.assertEqual(result.status_line, "approval accepted")
        self.assertEqual(
            result.outbound_events,
            [{"kind": "approval_response", "approved": True, "approval_id": "appr_123", "note": "Approved"}],
        )

    def test_router_handles_display_snapshot(self) -> None:
        router = CardputerRouter()
        message = CardputerMessage(
            CardputerMessageType.DISPLAY_SNAPSHOT,
            {"screen_text": "screen", "status_line": "Status: ok", "active_app": "Codex Buddy"},
        )

        result = router.route(message)

        self.assertEqual(result.status_line, "Status: ok")
        self.assertEqual(
            result.outbound_events,
            [
                {
                    "kind": "display_snapshot",
                    "screen_text": "screen",
                    "status_line": "Status: ok",
                    "active_app": "Codex Buddy",
                }
            ],
        )

    def test_router_handles_project_and_branch_selection(self) -> None:
        router = CardputerRouter()

        project_result = router.route(CardputerMessage(CardputerMessageType.PROJECT_SELECT, {"workspace_path": "C:/repo"}))
        branch_result = router.route(CardputerMessage(CardputerMessageType.BRANCH_SELECT, {"branch": "feature/cardputer"}))
        thread_result = router.route(CardputerMessage(CardputerMessageType.THREAD_SELECT, {"thread_id": "thr_123"}))

        self.assertEqual(project_result.status_line, "project selected: C:/repo")
        self.assertEqual(project_result.outbound_events, [{"kind": "project_select", "workspace_path": "C:/repo"}])
        self.assertEqual(branch_result.status_line, "branch selected: feature/cardputer")
        self.assertEqual(branch_result.outbound_events, [{"kind": "branch_select", "branch": "feature/cardputer"}])
        self.assertEqual(thread_result.status_line, "thread selected: thr_123")
        self.assertEqual(thread_result.outbound_events, [{"kind": "thread_select", "thread_id": "thr_123"}])

    def test_router_handles_bridge_prompt_messages(self) -> None:
        router = CardputerRouter()

        notification = router.route(
            CardputerMessage(CardputerMessageType.BRIDGE_NOTIFICATION, {"title": "Build finished", "detail": "Cardputer OS build is ready."})
        )
        question = router.route(
            CardputerMessage(
                CardputerMessageType.BRIDGE_QUESTION,
                {"title": "Choose workspace", "detail": "Select a target", "options": ["repo-a", "repo-b"]},
            )
        )
        confirmation = router.route(
            CardputerMessage(CardputerMessageType.BRIDGE_CONFIRMATION, {"title": "Restart", "detail": "Restart middleware?"})
        )
        response = router.route(
            CardputerMessage(CardputerMessageType.BRIDGE_RESPONSE, {"accepted": True, "selected_index": 1, "note": "Proceed"})
        )

        self.assertEqual(notification.status_line, "bridge notification: Build finished")
        self.assertEqual(
            notification.outbound_events,
            [
                {
                    "kind": "bridge_notification",
                    "title": "Build finished",
                    "detail": "Cardputer OS build is ready.",
                    "channel": "Build finished",
                    "urgency": "normal",
                }
            ],
        )
        self.assertEqual(question.status_line, "bridge question: Choose workspace")
        self.assertEqual(
            question.outbound_events,
            [
                {
                    "kind": "bridge_question",
                    "title": "Choose workspace",
                    "detail": "Select a target",
                    "options": ["repo-a", "repo-b"],
                    "channel": "cardputer.ask",
                }
            ],
        )
        self.assertEqual(confirmation.status_line, "bridge confirmation: Restart")
        self.assertEqual(
            confirmation.outbound_events,
            [
                {
                    "kind": "bridge_confirmation",
                    "title": "Restart",
                    "detail": "Restart middleware?",
                    "danger": False,
                    "channel": "cardputer.confirm",
                }
            ],
        )
        self.assertEqual(response.status_line, "bridge response accepted")
        self.assertEqual(
            response.outbound_events,
            [{"kind": "bridge_response", "accepted": True, "selected_index": 1, "note": "Proceed"}],
        )

    def test_message_from_dict_rejects_invalid_types(self) -> None:
        with self.assertRaises(ValueError):
            CardputerMessage.from_dict({"type": "unknown", "payload": {}})

    def test_message_from_dict_rejects_unsupported_protocol_version(self) -> None:
        with self.assertRaises(ValueError):
            CardputerMessage.from_dict({"protocol_version": 99, "type": "ping", "payload": {}})
