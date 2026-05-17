from __future__ import annotations

import json
from dataclasses import dataclass
import asyncio
from typing import Any, AsyncIterator

from .core import MiddlewareApp
from .events import Event, EventType
from .messages import CardputerMessage


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

    async def handle_connection(self, websocket: Any) -> None:
        queue: asyncio.Queue[str] = asyncio.Queue()

        def observer(events: list[Event]) -> None:
            for event in events:
                # We specifically want to push status and deltas to keep the firmware in sync
                if event.type in {EventType.CODEX_STATUS, EventType.CODEX_DELTA, EventType.CODEX_USAGE, EventType.APPROVAL_REQUEST, EventType.ERROR}:
                    # If it's a generic status, ensure we include the epoch
                    payload = dict(event.payload)
                    if event.type == EventType.CODEX_STATUS:
                        payload["state_epoch"] = self.app.session.state_epoch
                    
                    msg = json.dumps({
                        "type": event.type.value,
                        "payload": payload
                    }, ensure_ascii=False)
                    queue.put_nowait(msg)

        # Register the observer
        prev_observer = self.app.event_observer
        self.app.event_observer = observer

        # Push initial status
        await websocket.send(
            json.dumps(
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
        )

        async def push_loop():
            try:
                while True:
                    msg = await queue.get()
                    await websocket.send(msg)
                    queue.task_done()
            except Exception:
                pass

        push_task = asyncio.create_task(push_loop())

        try:
            async for raw_message in websocket:
                responses = await self.handle_raw_message(raw_message)
                for response in responses:
                    await websocket.send(response)
        finally:
            push_task.cancel()
            self.app.event_observer = prev_observer

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

        expected_token = self.app.config.bridge_token
        if expected_token is not None and message.auth_token != expected_token:
            return [
                self._ack(message.id, False),
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
