from __future__ import annotations

import contextlib
import json
from dataclasses import dataclass
import asyncio
from typing import Any, AsyncIterator

from websockets.exceptions import ConnectionClosed
from .bus import EventFilter
from .core import MiddlewareApp
from .events import Event, EventType
from .messages import CardputerMessage, CardputerMessageType

_DISCONNECT_EXCEPTIONS = (
    ConnectionClosed,
    ConnectionResetError,
    ConnectionAbortedError,
    BrokenPipeError,
    OSError,
)


@dataclass(slots=True)
class CardputerBridgeServer:
    app: MiddlewareApp

    def _ack(self, request_id: str | None, ok: bool) -> str:
        return json.dumps(
            {
                "type": "ack",
                "payload": {
                    "id": request_id,
                    "ok": ok,
                },
            },
            ensure_ascii=False,
        )

    def _bridge_connected_message(self) -> str:
        return json.dumps(
            {
                "type": "codex_status",
                "payload": {
                    "kind": "bridge_connected",
                    "state_epoch": self.app.session.state_epoch,
                    "workspace_path": self.app.session.workspace_path,
                    "branch": self.app.session.branch,
                    "thread_id": self.app.session.thread_id,
                },
            }
        )

    async def handle_connection(self, websocket: Any) -> None:
        subscriber_id = f"websocket-{id(websocket)}"
        filters = EventFilter(types={
            EventType.CODEX_STATUS,
            EventType.CODEX_DELTA,
            EventType.CODEX_USAGE,
            EventType.APPROVAL_REQUEST,
            EventType.ERROR,
        })
        event_queue = await self.app.event_bus.subscribe(subscriber_id, filters)

        push_task: asyncio.Task[None] | None = None

        async def push_loop() -> None:
            try:
                while True:
                    event = await event_queue.get()
                    try:
                        payload = dict(event.payload)
                        if event.type == EventType.CODEX_STATUS:
                            payload["state_epoch"] = self.app.session.state_epoch

                        msg = json.dumps({
                            "type": event.type.value,
                            "payload": payload
                        }, ensure_ascii=False)
                        await websocket.send(msg)
                    finally:
                        event_queue.task_done()
            except asyncio.CancelledError:
                raise
            except _DISCONNECT_EXCEPTIONS:
                return
            except Exception:
                return
        try:
            await websocket.send(self._bridge_connected_message())

            push_task = asyncio.create_task(push_loop())

            async for raw_message in websocket:
                responses = await self.handle_raw_message(raw_message)
                for response in responses:
                    try:
                        await websocket.send(response)
                    except _DISCONNECT_EXCEPTIONS:
                        return
        except _DISCONNECT_EXCEPTIONS:
            return
        finally:
            if push_task is not None:
                push_task.cancel()
                with contextlib.suppress(asyncio.CancelledError):
                    await push_task
            await self.app.event_bus.unsubscribe(subscriber_id)

    async def handle_raw_message(self, raw_message: str) -> list[str]:
        payload = json.loads(raw_message)
        if not isinstance(payload, dict):
            raise ValueError("Cardputer messages must be JSON objects.")

        request_id = payload.get("id")
        request_id_str = request_id if isinstance(request_id, str) else None

        try:
            message = CardputerMessage.from_dict(payload)
        except Exception:
            if request_id_str is not None:
                return [self._ack(request_id_str, False)]
            raise

        if message.type == CardputerMessageType.HELLO:
            return [
                json.dumps(
                    {
                        "type": "hello_ack",
                        "payload": {
                            "server": "cardputer-codex-middleware",
                            "version": "0.2.0",
                            "features": ["multi_run", "worktree_manager", "safe_mode", "yolo_worktree", "mcp", "websocket_audio", "ble_control"],
                        },
                        "id": f"ack-{message.id}",
                    },
                    ensure_ascii=False,
                )
            ]

        # Device security check
        # For now, we assume all devices with correct bridge_token are allowed
        # Future: implement device-id pairing and DB of trusted_devices

        expected_token = self.app.config.bridge_token
        if expected_token is not None and message.auth_token != expected_token:
            return [
                self._ack(message.id, False),
            ]

        # Temporary device policy (simulated pairing)
        simulated_device_policy = {
            "allowed_actions": ["status", "approve", "reject", "interrupt", "voice_prompt", "ping", "hello"],
            "denied_actions": ["change_policy", "enable_yolo_global"]
        }

        # Device RBAC check
        # Device RBAC check
        request_info = {"action": message.type.value, "title": message.payload.get("title", "")}

        # Handle cases where app doesn't have policy_manager (e.g. some tests)
        policy_manager = getattr(self.app, "policy_manager", None)
        if policy_manager is not None:
            denied, reason = policy_manager.evaluate_request_for_device(simulated_device_policy, request_info)
            if denied is False:
                 return [
                    self._ack(message.id, False),
                    json.dumps({"type": "error", "payload": {"message": reason}}, ensure_ascii=False)
                ]
        try:
            events = await self.app.handle_cardputer_message(message)
        except Exception:
            return [self._ack(message.id, False)]

        return [self._ack(message.id, True)] + [json.dumps(event.to_dict(), ensure_ascii=False) for event in events]

    async def iter_responses(self, messages: AsyncIterator[str]) -> AsyncIterator[str]:
        async for raw_message in messages:
            for response in await self.handle_raw_message(raw_message):
                yield response
