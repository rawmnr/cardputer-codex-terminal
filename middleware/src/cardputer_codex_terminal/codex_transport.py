from __future__ import annotations

from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import AsyncIterator


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
    def __init__(self, ws_url: str) -> None:
        self.ws_url = ws_url

    async def initialize(self) -> None:
        raise NotImplementedError(
            "Le transport WebSocket Codex n'est pas encore branche. "
            f"URL cible actuelle: {self.ws_url}"
        )

    async def start_turn(self, prompt: str) -> AsyncIterator[CodexReply]:
        raise NotImplementedError("Le transport WebSocket Codex n'est pas encore branche.")
