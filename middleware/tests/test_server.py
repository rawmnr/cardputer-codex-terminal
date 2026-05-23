from __future__ import annotations

import asyncio
import json
import unittest
from types import SimpleNamespace

from cardputer_codex_terminal.ble_bridge import CardputerBleBridge, SERVICE_UUID
from cardputer_codex_terminal.events import Event, EventType
from cardputer_codex_terminal.messages import CardputerMessage, CardputerMessageType
from cardputer_codex_terminal.server import CardputerBridgeServer


class BridgeServerTests(unittest.TestCase):
    def test_handle_raw_message_routes_and_serializes_events(self) -> None:
        class FakeApp:
            def __init__(self) -> None:
                self.session = SimpleNamespace(workspace_path="C:/repo", branch="feature/cardputer", thread_id="thr_123")
                self.config = SimpleNamespace(bridge_token=None)
                self.received: CardputerMessage | None = None

            async def handle_cardputer_message(self, message: CardputerMessage) -> list[Event]:
                self.received = message
                return [Event(EventType.CODEX_STATUS, {"kind": "bridge_status", "content": "ok"})]

        async def scenario() -> tuple[CardputerMessage | None, list[str]]:
            server = CardputerBridgeServer(FakeApp())
            payload = CardputerMessage(CardputerMessageType.STATUS_REQUEST, {}).to_dict()
            responses = await server.handle_raw_message(json.dumps(payload))
            return server.app.received, responses, payload["id"]  # type: ignore[return-value]

        received, responses, request_id = asyncio.run(scenario())

        self.assertIsNotNone(received)
        self.assertEqual(received.type, CardputerMessageType.STATUS_REQUEST)
        self.assertEqual(
            responses,
            [
                json.dumps({"type": "ack", "payload": {"id": request_id, "ok": True}}, ensure_ascii=False),
                json.dumps(
                    {
                        "type": EventType.CODEX_STATUS.value,
                        "payload": {"kind": "bridge_status", "content": "ok"},
                    },
                    ensure_ascii=False,
                ),
            ],
        )

    def test_handle_raw_message_rejects_invalid_bridge_token(self) -> None:
        class FakeApp:
            def __init__(self) -> None:
                self.session = SimpleNamespace(workspace_path="C:/repo", branch="feature/cardputer", thread_id="thr_123")
                self.config = SimpleNamespace(bridge_token="secret")
                self.received: CardputerMessage | None = None

            async def handle_cardputer_message(self, message: CardputerMessage) -> list[Event]:
                self.received = message
                return [Event(EventType.CODEX_STATUS, {"kind": "bridge_status", "content": "ok"})]

        async def scenario() -> tuple[CardputerMessage | None, list[str]]:
            server = CardputerBridgeServer(FakeApp())
            payload = CardputerMessage(CardputerMessageType.STATUS_REQUEST, {}, auth_token="wrong").to_dict()
            responses = await server.handle_raw_message(json.dumps(payload))
            return server.app.received, responses, payload["id"]  # type: ignore[return-value]

        received, responses, request_id = asyncio.run(scenario())

        self.assertIsNone(received)
        self.assertEqual(
            responses,
            [json.dumps({"type": "ack", "payload": {"id": request_id, "ok": False}}, ensure_ascii=False)],
        )


class BleBridgeTests(unittest.TestCase):
    def test_frame_line_chunks_utf8_and_newline(self) -> None:
        bridge = CardputerBleBridge(SimpleNamespace(event_bus=None), chunk_size=5)

        chunks = bridge._frame_line("abcdef")

        self.assertTrue(all(len(chunk) <= 5 for chunk in chunks))
        self.assertEqual(b"".join(chunks), b"abcdef\n")

    def test_select_device_matches_name_prefix_and_service_uuid(self) -> None:
        bridge = CardputerBleBridge(SimpleNamespace(event_bus=None))
        named_device = SimpleNamespace(name="CardputerCodex_ABC123", address="addr-name")
        named_adv = SimpleNamespace(local_name="CardputerCodex_ABC123", service_uuids=[])
        service_device = SimpleNamespace(name="Other", address="addr-service")
        service_adv = SimpleNamespace(local_name="Other", service_uuids=[SERVICE_UUID])

        selected_name = bridge.select_device([(service_device, SimpleNamespace(local_name="Other", service_uuids=[])), (named_device, named_adv)])
        selected_service = bridge.select_device([(service_device, service_adv)])

        self.assertIs(selected_name, named_device)
        self.assertIs(selected_service, service_device)

    def test_should_forward_only_control_plane_events(self) -> None:
        bridge = CardputerBleBridge(SimpleNamespace(event_bus=None))

        self.assertTrue(bridge._should_forward_event(Event(EventType.APPROVAL_REQUEST, {"kind": "approval_request"})))
        self.assertTrue(bridge._should_forward_event(Event(EventType.CODEX_STATUS, {"kind": "bridge_connected"})))
        self.assertTrue(bridge._should_forward_event(Event(EventType.STATUS_SNAPSHOT, {"summary": "ok"})))
        self.assertFalse(bridge._should_forward_event(Event(EventType.CODEX_STATUS, {"kind": "display_snapshot"})))
        self.assertFalse(bridge._should_forward_event(Event(EventType.CODEX_DELTA, {"kind": "delta"})))
