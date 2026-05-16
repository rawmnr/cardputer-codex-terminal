from __future__ import annotations

from dataclasses import dataclass, field

from .config import AppConfig
from .codex_transport import CodexTransport, LocalWebSocketCodexTransport, MockCodexTransport
from .events import Event, EventType


@dataclass(slots=True)
class MiddlewareApp:
    config: AppConfig
    transport: CodexTransport = field(init=False)

    def __post_init__(self) -> None:
        self.transport = (
            MockCodexTransport()
            if self.config.use_mock_codex
            else LocalWebSocketCodexTransport(self.config.codex_ws_url)
        )

    async def initialize(self) -> None:
        await self.transport.initialize()

    async def handle_text_prompt(self, text: str) -> list[Event]:
        events: list[Event] = []
        async for reply in self.transport.start_turn(text):
            events.append(
                Event(
                    EventType.CODEX_DELTA if reply.kind == "delta" else EventType.CODEX_STATUS,
                    {"content": reply.content, "kind": reply.kind},
                )
            )
        return events
