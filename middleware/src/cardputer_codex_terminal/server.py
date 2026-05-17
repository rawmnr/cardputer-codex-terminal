from __future__ import annotations

import json
from dataclasses import dataclass
from typing import Any, AsyncIterator

from .core import MiddlewareApp
from .events import Event
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
        await websocket.send(
            json.dumps(
                {
                    "type": "codex_status",
                    "payload": {
                        "kind": "bridge_connected",
                        "workspace_path": self.app.session.workspace_path,
                        "branch": self.app.session.branch,
                        "thread_id": self.app.session.thread_id,
                    },
                }
            )
        )

        async for raw_message in websocket:
            responses = await self.handle_raw_message(raw_message)
            for response in responses:
                await websocket.send(response)

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
