from __future__ import annotations

from dataclasses import dataclass, field

from .config import AppConfig
from .codex_transport import CodexTransport, LocalWebSocketCodexTransport, MockCodexTransport
from .events import Event, EventType
from .messages import CardputerMessage, CardputerMessageType
from .router import CardputerRouter


@dataclass(slots=True)
class MiddlewareApp:
    config: AppConfig
    transport: CodexTransport = field(init=False)
    router: CardputerRouter = field(default_factory=CardputerRouter)

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

    async def handle_cardputer_message(self, message: CardputerMessage) -> list[Event]:
        routed = self.router.route(message)

        if message.type == CardputerMessageType.TEXT_PROMPT:
            return await self.handle_text_prompt(str(message.payload.get("text", "")))

        return [Event(EventType.CODEX_STATUS, {"content": routed.status_line, "kind": "router"})]
