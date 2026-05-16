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

    def test_router_handles_text_prompt(self) -> None:
        router = CardputerRouter()
        message = CardputerMessage(CardputerMessageType.TEXT_PROMPT, {"text": "hello"})

        result = router.route(message)

        self.assertEqual(result.status_line, "text prompt received")
        self.assertEqual(result.outbound_events, [{"kind": "text_prompt", "text": "hello"}])

    def test_router_handles_approval_response(self) -> None:
        router = CardputerRouter()
        message = CardputerMessage(CardputerMessageType.APPROVAL_RESPONSE, {"approved": True})

        result = router.route(message)

        self.assertEqual(result.status_line, "approval accepted")
        self.assertEqual(result.outbound_events, [{"kind": "approval_response", "approved": True}])

    def test_message_from_dict_rejects_invalid_types(self) -> None:
        with self.assertRaises(ValueError):
            CardputerMessage.from_dict({"type": "unknown", "payload": {}})

