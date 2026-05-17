from __future__ import annotations

from dataclasses import dataclass, field
from typing import Callable

from .config import AppConfig
from .codex_transport import CodexTransport, LocalWebSocketCodexTransport, MockCodexTransport, StdioCodexAppServerTransport
from .events import Event, EventType
from .messages import CardputerMessage, CardputerMessageType
from .router import CardputerRouter
from .session import CodexSession
from .voice import MockVoiceTranscriber, VoicePromptBuffer, VoiceTranscriber


@dataclass(slots=True)
class MiddlewareApp:
    config: AppConfig
    transport: CodexTransport = field(init=False)
    router: CardputerRouter = field(default_factory=CardputerRouter)
    session: CodexSession = field(init=False)
    voice_buffer: VoicePromptBuffer = field(default_factory=VoicePromptBuffer)
    transcriber: VoiceTranscriber = field(default_factory=MockVoiceTranscriber)
    event_observer: Callable[[list[Event]], None] | None = field(default=None, repr=False, compare=False)

    def __post_init__(self) -> None:
        transport_kind = self.config.codex_transport
        if not self.config.use_mock_codex and transport_kind == "mock":
            transport_kind = "stdio"

        if transport_kind == "mock":
            self.transport = MockCodexTransport()
        elif transport_kind == "websocket":
            self.transport = LocalWebSocketCodexTransport(self.config.codex_ws_url)
        else:
            self.transport = StdioCodexAppServerTransport(list(self.config.codex_command), cwd=self.config.workspace_path)
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
        event = Event(
            EventType.CODEX_STATUS,
            {"kind": "project_selected", "content": workspace_path, "workspace_path": workspace_path},
        )
        self._notify([event])
        return event

    async def select_branch(self, branch: str) -> Event:
        self.session.branch = branch
        if self.session.thread_id is not None:
            await self.transport.update_thread_metadata(self.session.thread_id, branch=branch)
        event = Event(EventType.CODEX_STATUS, {"kind": "branch_selected", "content": branch, "branch": branch})
        self._notify([event])
        return event

    async def select_thread(self, thread_id: str) -> Event:
        self.session.thread_id = await self.transport.resume_thread(thread_id)
        event = Event(EventType.CODEX_STATUS, {"kind": "thread_selected", "content": thread_id, "thread_id": thread_id})
        self._notify([event])
        return event

    def _set_bridge_prompt(self, kind: str, title: str, detail: str, options: tuple[str, ...] = ()) -> Event:
        self.session.bridge_prompt_kind = kind
        self.session.bridge_prompt_title = title
        self.session.bridge_prompt_detail = detail
        self.session.bridge_prompt_options = options
        self.session.bridge_prompt_selected_index = 0
        event = Event(
            EventType.CODEX_STATUS,
            {
                "kind": kind,
                "content": title,
                "title": title,
                "detail": detail,
                "options": list(options),
            },
        )
        self._notify([event])
        return event

    def append_audio_chunk(self, pcm_b64: str, chunk_id: int | None = None) -> Event:
        sample_count = self.voice_buffer.append_chunk(pcm_b64)
        event = Event(
            EventType.AUDIO_CHUNK,
            {
                "chunk_id": chunk_id,
                "sample_count": sample_count,
                "chunk_count": self.voice_buffer.chunk_count,
                "byte_count": self.voice_buffer.byte_count(),
            },
        )
        self._notify([event])
        return event

    async def finalize_voice_prompt(self, sample_rate_hz: int) -> list[Event]:
        self.voice_buffer.sample_rate_hz = sample_rate_hz
        if not self.voice_buffer.has_audio():
            event = Event(EventType.ERROR, {"content": "No voice audio is buffered."})
            self._notify([event])
            return [event]

        transcript = await self.voice_buffer.transcribe(self.transcriber)
        events = [Event(EventType.CODEX_STATUS, {"kind": "voice_prompt_transcribed", "content": transcript})]
        events.extend(await self._collect_text_prompt_events(transcript))
        self._notify(events)
        return events

    def _set_pending_approval(self, data: dict[str, object]) -> Event:
        approval_id = str(data.get("approval_id") or data.get("approvalId") or "")
        title = str(data.get("title") or data.get("summary") or data.get("command") or "Approval requested")
        detail = str(data.get("detail") or data.get("message") or data.get("reason") or "")
        timeout_seconds_raw = data.get("timeoutSeconds") or data.get("timeout_seconds")
        timeout_seconds = int(timeout_seconds_raw) if isinstance(timeout_seconds_raw, int) else None

        self.session.pending_approval_id = approval_id or None
        self.session.pending_approval_title = title
        self.session.pending_approval_detail = detail
        self.session.pending_approval_timeout_seconds = timeout_seconds

        event = Event(
            EventType.APPROVAL_REQUEST,
            {
                "approval_id": self.session.pending_approval_id,
                "title": title,
                "detail": detail,
                "timeout_seconds": timeout_seconds,
            },
        )
        return event

    async def handle_approval_response(self, approved: bool, approval_id: str | None = None, note: str | None = None) -> Event:
        pending_id = approval_id or self.session.pending_approval_id
        if pending_id is None:
            event = Event(EventType.ERROR, {"content": "No approval is pending."})
            self._notify([event])
            return event

        await self.transport.submit_approval(pending_id, approved, note=note)
        self.session.pending_approval_id = None
        self.session.pending_approval_title = None
        self.session.pending_approval_detail = None
        self.session.pending_approval_timeout_seconds = None
        event = Event(
            EventType.APPROVAL_RESPONSE,
            {
                "approval_id": pending_id,
                "approved": approved,
                "note": note,
            },
        )
        self._notify([event])
        return event

    async def _ensure_thread(self) -> None:
        if self.session.thread_id:
            return
        self.session.thread_id = await self.transport.start_thread(self.session.workspace_path, self.session.branch)

    async def handle_text_prompt(self, text: str) -> list[Event]:
        events = await self._collect_text_prompt_events(text)
        self._notify(events)
        return events

    async def _collect_text_prompt_events(self, text: str) -> list[Event]:
        await self._ensure_thread()
        events: list[Event] = []
        async for reply in self.transport.start_turn(text, thread_id=self.session.thread_id, cwd=self.session.workspace_path):
            if reply.kind == "approval_request":
                events.append(self._set_pending_approval(reply.data))
                break
            events.append(
                Event(
                    EventType.CODEX_DELTA
                    if reply.kind == "delta"
                    else EventType.CODEX_USAGE
                    if reply.kind == "usage"
                    else EventType.CODEX_STATUS,
                    {"content": reply.content, "kind": reply.kind, "data": reply.data},
                )
            )
        return events

    async def handle_cardputer_message(self, message: CardputerMessage) -> list[Event]:
        routed = self.router.route(message)

        if message.type == CardputerMessageType.TEXT_PROMPT:
            return await self.handle_text_prompt(str(message.payload.get("text", "")))

        if message.type == CardputerMessageType.PROJECT_SELECT:
            return [await self.select_workspace(str(message.payload.get("workspace_path", ".")))]

        if message.type == CardputerMessageType.AUDIO_CHUNK:
            return [
                self.append_audio_chunk(
                    str(message.payload.get("pcm_b64", "")),
                    chunk_id=int(message.payload.get("chunk_id", 0)),
                )
            ]

        if message.type == CardputerMessageType.VOICE_PROMPT_READY:
            return await self.finalize_voice_prompt(int(message.payload.get("sample_rate_hz", 16000)))

        if message.type == CardputerMessageType.BRANCH_SELECT:
            return [await self.select_branch(str(message.payload.get("branch", "")))]

        if message.type == CardputerMessageType.THREAD_SELECT:
            return [await self.select_thread(str(message.payload.get("thread_id", "")))]

        if message.type == CardputerMessageType.APPROVAL_RESPONSE:
            return [
                await self.handle_approval_response(
                    bool(message.payload.get("approved", False)),
                    approval_id=str(message.payload.get("approval_id") or message.payload.get("approvalId") or ""),
                    note=(str(message.payload.get("note")) if message.payload.get("note") is not None else None),
                )
            ]

        if message.type == CardputerMessageType.DISPLAY_SNAPSHOT:
            payload = {
                "kind": "display_snapshot",
                "content": str(message.payload.get("status_line") or message.payload.get("content") or ""),
                "screen_text": str(message.payload.get("screen_text") or ""),
                "active_app": str(message.payload.get("active_app") or ""),
                "status_line": str(message.payload.get("status_line") or message.payload.get("content") or ""),
            }
            for key in ("firmware_name", "network_status_line", "input_line"):
                if message.payload.get(key):
                    payload[key] = str(message.payload[key])
            event = Event(
                EventType.DISPLAY_SNAPSHOT,
                payload,
            )
            self._notify([event])
            return [event]

        if message.type == CardputerMessageType.BRIDGE_NOTIFICATION:
            title = str(message.payload.get("title") or "Notification")
            detail = str(message.payload.get("detail") or "")
            return [self._set_bridge_prompt("bridge_notification", title, detail)]

        if message.type == CardputerMessageType.BRIDGE_QUESTION:
            title = str(message.payload.get("title") or "Question")
            detail = str(message.payload.get("detail") or "")
            options = tuple(str(option) for option in (message.payload.get("options") or [])[:3])
            return [self._set_bridge_prompt("bridge_question", title, detail, options)]

        if message.type == CardputerMessageType.BRIDGE_CONFIRMATION:
            title = str(message.payload.get("title") or "Confirmation")
            detail = str(message.payload.get("detail") or "")
            return [self._set_bridge_prompt("bridge_confirmation", title, detail, ("Accept", "Reject"))]

        if message.type == CardputerMessageType.BRIDGE_RESPONSE:
            accepted = bool(message.payload.get("accepted", False))
            selected_index = int(message.payload.get("selected_index", 0))
            note = str(message.payload.get("note") or "")
            if self.session.bridge_prompt_kind is None:
                event = Event(EventType.ERROR, {"content": "No bridge prompt is pending."})
                self._notify([event])
                return [event]
            pending_kind = self.session.bridge_prompt_kind
            self.session.bridge_prompt_kind = None
            self.session.bridge_prompt_title = None
            self.session.bridge_prompt_detail = None
            self.session.bridge_prompt_options = ()
            self.session.bridge_prompt_selected_index = 0
            event = Event(
                EventType.CODEX_STATUS,
                {
                    "kind": "bridge_response",
                    "content": "bridge response recorded",
                    "accepted": accepted,
                    "selected_index": selected_index,
                    "note": note,
                    "prompt_kind": pending_kind,
                },
            )
            self._notify([event])
            return [event]

        if message.type == CardputerMessageType.STATUS_REQUEST:
            event = Event(
                EventType.CODEX_STATUS,
                {
                    "kind": "session_status",
                    "workspace_path": self.session.workspace_path,
                    "branch": self.session.branch,
                    "thread_id": self.session.thread_id,
                    "approval_id": self.session.pending_approval_id,
                    "approval_title": self.session.pending_approval_title,
                    "approval_detail": self.session.pending_approval_detail,
                    "bridge_prompt_kind": self.session.bridge_prompt_kind,
                    "bridge_prompt_title": self.session.bridge_prompt_title,
                    "bridge_prompt_detail": self.session.bridge_prompt_detail,
                    "bridge_prompt_options": list(self.session.bridge_prompt_options),
                    "bridge_prompt_selected_index": self.session.bridge_prompt_selected_index,
                },
            )
            self._notify([event])
            return [event]

        event = Event(EventType.CODEX_STATUS, {"content": routed.status_line, "kind": "router"})
        self._notify([event])
        return [event]

    def _notify(self, events: list[Event]) -> None:
        if self.event_observer is None or not events:
            return
        self.event_observer(events)
