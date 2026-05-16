from __future__ import annotations

from dataclasses import dataclass, field

from .config import AppConfig
from .codex_transport import CodexTransport, LocalWebSocketCodexTransport, MockCodexTransport
from .events import Event, EventType
from .messages import CardputerMessage, CardputerMessageType
from .router import CardputerRouter
from .session import CodexSession


@dataclass(slots=True)
class MiddlewareApp:
    config: AppConfig
    transport: CodexTransport = field(init=False)
    router: CardputerRouter = field(default_factory=CardputerRouter)
    session: CodexSession = field(init=False)

    def __post_init__(self) -> None:
        self.transport = (
            MockCodexTransport()
            if self.config.use_mock_codex
            else LocalWebSocketCodexTransport(self.config.codex_ws_url)
        )
        self.session = CodexSession(
            workspace_path=self.config.workspace_path,
            branch=self.config.branch,
            thread_id=self.config.thread_id,
        )

    async def initialize(self) -> None:
        await self.transport.initialize()

    async def select_workspace(self, workspace_path: str) -> Event:
        self.session.workspace_path = workspace_path
        self.session.thread_id = None
        return Event(
            EventType.CODEX_STATUS,
            {"kind": "project_selected", "content": workspace_path, "workspace_path": workspace_path},
        )

    async def select_branch(self, branch: str) -> Event:
        self.session.branch = branch
        if self.session.thread_id is not None:
            await self.transport.update_thread_metadata(self.session.thread_id, branch=branch)
        return Event(EventType.CODEX_STATUS, {"kind": "branch_selected", "content": branch, "branch": branch})

    async def select_thread(self, thread_id: str) -> Event:
        self.session.thread_id = await self.transport.resume_thread(thread_id)
        return Event(EventType.CODEX_STATUS, {"kind": "thread_selected", "content": thread_id, "thread_id": thread_id})

    async def _ensure_thread(self) -> None:
        if self.session.thread_id:
            return
        self.session.thread_id = await self.transport.start_thread(self.session.workspace_path, self.session.branch)

    async def handle_text_prompt(self, text: str) -> list[Event]:
        await self._ensure_thread()
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

        if message.type == CardputerMessageType.PROJECT_SELECT:
            return [await self.select_workspace(str(message.payload.get("workspace_path", ".")))]

        if message.type == CardputerMessageType.BRANCH_SELECT:
            return [await self.select_branch(str(message.payload.get("branch", "")))]

        if message.type == CardputerMessageType.THREAD_SELECT:
            return [await self.select_thread(str(message.payload.get("thread_id", "")))]

        if message.type == CardputerMessageType.STATUS_REQUEST:
            return [
                Event(
                    EventType.CODEX_STATUS,
                    {
                        "kind": "session_status",
                        "workspace_path": self.session.workspace_path,
                        "branch": self.session.branch,
                        "thread_id": self.session.thread_id,
                    },
                )
            ]

        return [Event(EventType.CODEX_STATUS, {"content": routed.status_line, "kind": "router"})]
