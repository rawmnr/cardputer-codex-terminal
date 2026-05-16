from __future__ import annotations

from abc import ABC, abstractmethod
from dataclasses import dataclass
import json
from typing import AsyncIterator
from typing import Callable, Awaitable, Any


WebSocketFactory = Callable[[str], Awaitable[Any]]


@dataclass(slots=True)
class CodexReply:
    kind: str
    content: str


class CodexTransport(ABC):
    @abstractmethod
    async def initialize(self) -> None:
        raise NotImplementedError

    @abstractmethod
    async def start_turn(self, prompt: str) -> AsyncIterator[CodexReply]:
        raise NotImplementedError


class MockCodexTransport(CodexTransport):
    async def initialize(self) -> None:
        return None

    async def start_turn(self, prompt: str) -> AsyncIterator[CodexReply]:
        yield CodexReply("status", "mock_codex_ready")
        yield CodexReply("delta", f"Received: {prompt}")
        yield CodexReply("delta", "This transport is a local placeholder.")
        yield CodexReply("completed", "mock_turn_completed")


class LocalWebSocketCodexTransport(CodexTransport):
    def __init__(self, ws_url: str, connect_factory: WebSocketFactory | None = None) -> None:
        self.ws_url = ws_url
        self._connect_factory = connect_factory
        self._ws: Any | None = None
        self._initialized = False

    async def initialize(self) -> None:
        ws = await self._ensure_connection()
        await ws.send(
            json.dumps(
                {
                    "id": "initialize-1",
                    "method": "initialize",
                    "params": {
                        "clientInfo": {"name": "cardputer-codex-middleware", "version": "0.1.0"},
                        "capabilities": {"experimentalApi": True},
                    },
                }
            )
        )

        response = await self._read_json(ws)
        if response.get("error") is not None:
            raise RuntimeError(f"Codex initialization failed: {response['error']}")

        self._initialized = True

    async def start_turn(self, prompt: str) -> AsyncIterator[CodexReply]:
        ws = await self._ensure_connection()
        if not self._initialized:
            await self.initialize()
            ws = await self._ensure_connection()

        await ws.send(
            json.dumps(
                {
                    "id": "turn-start-1",
                    "method": "turn/start",
                    "params": {"text": prompt},
                }
            )
        )

        while True:
            message = await self._read_json(ws)
            reply = self._decode_reply(message)
            if reply is None:
                continue
            yield reply
            if reply.kind == "completed":
                return

    async def close(self) -> None:
        if self._ws is not None:
            close = getattr(self._ws, "close", None)
            if callable(close):
                result = close()
                if hasattr(result, "__await__"):
                    await result
        self._ws = None
        self._initialized = False

    async def _ensure_connection(self) -> Any:
        if self._ws is not None:
            return self._ws

        if self._connect_factory is not None:
            self._ws = await self._connect_factory(self.ws_url)
            return self._ws

        import websockets  # type: ignore

        self._ws = await websockets.connect(self.ws_url)
        return self._ws

    async def _read_json(self, ws: Any) -> dict[str, Any]:
        raw = await ws.recv()
        if isinstance(raw, bytes):
            raw = raw.decode("utf-8")
        if not isinstance(raw, str):
            raise TypeError("WebSocket messages must decode to text JSON.")
        data = json.loads(raw)
        if not isinstance(data, dict):
            raise TypeError("WebSocket JSON messages must be objects.")
        return data

    def _decode_reply(self, message: dict[str, Any]) -> CodexReply | None:
        kind = message.get("kind") or message.get("type") or message.get("event")
        content = message.get("content") or message.get("text") or message.get("message") or ""

        if kind in {"status", "turn/status"}:
            return CodexReply("status", str(content))
        if kind in {"delta", "item/agentMessage/delta"}:
            return CodexReply("delta", str(content))
        if kind in {"completed", "turn/completed"}:
            return CodexReply("completed", str(content or "turn_completed"))
        if kind in {"error", "turn/error"}:
            return CodexReply("status", f"error: {content}")
        return None
