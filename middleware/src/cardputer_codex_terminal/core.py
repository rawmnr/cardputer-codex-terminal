from __future__ import annotations

import asyncio
from dataclasses import dataclass, field
from typing import Any, Callable

from .config import AppConfig
from .codex_transport import CodexTransport, LocalWebSocketCodexTransport, MockCodexTransport, StdioCodexAppServerTransport
from .events import Event, EventType
from .messages import CardputerMessage, CardputerMessageType
from .router import CardputerRouter
from .runs import AgentRun, RunIndex, RunMode, RunRole
from .session import SessionIndex, SessionState
from .voice import FasterWhisperVoiceTranscriber, MockVoiceTranscriber, VoicePromptBuffer, VoiceTranscriber


@dataclass(slots=True)
class MiddlewareApp:
    config: AppConfig
    transport: CodexTransport = field(init=False)
    router: CardputerRouter = field(default_factory=CardputerRouter)
    session_index: SessionIndex = field(default_factory=SessionIndex)
    run_index: RunIndex = field(default_factory=RunIndex)
    session: SessionState = field(init=False)
    voice_buffer: VoicePromptBuffer = field(default_factory=VoicePromptBuffer)
    transcriber: VoiceTranscriber = field(default_factory=MockVoiceTranscriber)
    event_observer: Callable[[list[Event]], None] | None = field(default=None, repr=False, compare=False)
    _bridge_prompt_lock: asyncio.Lock = field(default_factory=asyncio.Lock, repr=False, compare=False)
    _bridge_prompt_future: asyncio.Future[dict[str, Any]] | None = field(default=None, init=False, repr=False, compare=False)

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
        self.session = self.session_index.ensure_active(
            workspace_path=self.config.workspace_path,
            branch=self.config.branch,
            thread_id=self.config.thread_id,
            title=self.config.thread_id,
        )
        self._ensure_run_for_session(self.session, sync=True)

    def _ensure_run_for_session(self, session: SessionState, *, sync: bool = False) -> AgentRun:
        run = self.run_index.find_by_session_id(session.session_id)
        if run is None and session.run_id is not None:
            run = self.run_index.runs.get(session.run_id)
        if run is None:
            run = self.run_index.create_run(
                workspace_path=session.workspace_path,
                base_branch=self.config.branch or session.branch or "main",
                branch=session.branch,
                thread_id=session.thread_id,
                session_id=session.session_id,
                role=RunRole.MAIN,
                mode=RunMode.SAFE,
            )
            session.run_id = run.run_id
        else:
            session.run_id = run.run_id
            if sync:
                run.touch(
                    workspace_path=session.workspace_path,
                    base_branch=self.config.branch or session.branch or run.base_branch,
                    branch=session.branch,
                    thread_id=session.thread_id,
                    session_id=session.session_id,
                )
        self.run_index.select(run.run_id)
        return run


    async def initialize(self) -> None:
        await self.transport.initialize()

    async def select_workspace(self, workspace_path: str) -> Event:
        self.session.workspace_path = workspace_path
        self.session.thread_id = None
        self._ensure_run_for_session(self.session, sync=True)
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
        self._ensure_run_for_session(self.session, sync=True)
        event = Event(EventType.CODEX_STATUS, {"kind": "branch_selected", "content": branch, "branch": branch})
        self._notify([event])
        return event

    async def select_thread(self, thread_id: str) -> Event:
        resumed_thread_id = await self.transport.resume_thread(thread_id)
        session = self.session_index.find_by_thread_id(resumed_thread_id)
        if session is None:
            session = self.session_index.create_session(
                workspace_path=self.session.workspace_path,
                branch=self.session.branch,
                thread_id=resumed_thread_id,
                title=resumed_thread_id,
            )
        else:
            session.touch(workspace_path=self.session.workspace_path, branch=self.session.branch, thread_id=resumed_thread_id)
        self.session = session
        self._ensure_run_for_session(self.session, sync=True)
        event = Event(EventType.CODEX_STATUS, {"kind": "thread_selected", "content": thread_id, "thread_id": thread_id})
        self._notify([event])
        return event

    def _set_bridge_prompt(self, kind: str, title: str, detail: str, options: tuple[str, ...] = ()) -> Event:
        return self._set_bridge_prompt_with_metadata(kind, title, detail, options=options)

    def _set_bridge_prompt_with_metadata(
        self,
        kind: str,
        title: str,
        detail: str,
        *,
        options: tuple[str, ...] = (),
        channel: str | None = None,
        urgency: str | None = None,
        danger: bool | None = None,
    ) -> Event:
        if self._bridge_prompt_future is not None and not self._bridge_prompt_future.done():
            raise RuntimeError("A bridge prompt is already pending.")
        self.session.bridge_prompt_kind = kind
        self.session.bridge_prompt_title = title
        self.session.bridge_prompt_detail = detail
        self.session.bridge_prompt_channel = channel
        self.session.bridge_prompt_urgency = urgency
        self.session.bridge_prompt_danger = danger
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
                **({"channel": channel} if channel is not None else {}),
                **({"urgency": urgency} if urgency is not None else {}),
                **({"danger": danger} if danger is not None else {}),
            },
        )
        self._notify([event])
        return event

    def _clear_bridge_prompt(self) -> None:
        self.session.bridge_prompt_kind = None
        self.session.bridge_prompt_title = None
        self.session.bridge_prompt_detail = None
        self.session.bridge_prompt_channel = None
        self.session.bridge_prompt_urgency = None
        self.session.bridge_prompt_danger = None
        self.session.bridge_prompt_options = ()
        self.session.bridge_prompt_selected_index = 0

    def _start_bridge_prompt_waiter(self) -> asyncio.Future[dict[str, Any]]:
        loop = asyncio.get_running_loop()
        if self._bridge_prompt_future is not None and not self._bridge_prompt_future.done():
            raise RuntimeError("A bridge prompt is already pending.")
        self._bridge_prompt_future = loop.create_future()
        return self._bridge_prompt_future

    def _complete_bridge_prompt(self, accepted: bool, selected_index: int, note: str) -> None:
        future = self._bridge_prompt_future
        prompt_kind = self.session.bridge_prompt_kind
        prompt_title = self.session.bridge_prompt_title
        prompt_detail = self.session.bridge_prompt_detail
        prompt_options = self.session.bridge_prompt_options
        prompt_channel = self.session.bridge_prompt_channel
        prompt_urgency = self.session.bridge_prompt_urgency
        prompt_danger = self.session.bridge_prompt_danger

        self._clear_bridge_prompt()
        if future is not None and not future.done():
            future.set_result(
                {
                    "status": "answered",
                    "accepted": accepted,
                    "selected_index": selected_index,
                    "note": note,
                    "prompt_kind": prompt_kind,
                    "title": prompt_title,
                    "detail": prompt_detail,
                    "options": list(prompt_options),
                    "channel": prompt_channel,
                    "urgency": prompt_urgency,
                    "danger": prompt_danger,
                }
            )
            self._bridge_prompt_future = None

    async def _await_bridge_prompt_response(self, timeout_s: int) -> dict[str, Any]:
        future = self._bridge_prompt_future
        if future is None:
            raise RuntimeError("A bridge prompt is not pending.")

        try:
            return await asyncio.wait_for(future, timeout=timeout_s)
        except TimeoutError:
            if self._bridge_prompt_future is future:
                self._bridge_prompt_future = None
            self._clear_bridge_prompt()
            return {
                "status": "timeout",
                "timed_out": True,
                "accepted": False,
                "selected_index": None,
                "note": "",
            }

    async def notify_cardputer(self, title: str, body: str, urgency: str) -> Event:
        async with self._bridge_prompt_lock:
            event = self._set_bridge_prompt_with_metadata(
                "bridge_notification",
                title,
                body,
                channel=title,
                urgency=urgency,
            )
        return event

    async def show_cardputer(self, text: str, channel: str) -> Event:
        async with self._bridge_prompt_lock:
            event = self._set_bridge_prompt_with_metadata(
                "bridge_notification",
                channel or "Show",
                text,
                channel=channel or None,
                urgency="normal",
            )
        return event

    async def ask_cardputer(self, question: str, choices: list[str], timeout_s: int) -> dict[str, Any]:
        if not choices or len(choices) > 3:
            raise ValueError("Ask requires between 1 and 3 choices.")
        if timeout_s < 1:
            raise ValueError("Ask timeout must be a positive integer.")

        async with self._bridge_prompt_lock:
            self._set_bridge_prompt_with_metadata(
                "bridge_question",
                question,
                question,
                options=tuple(choices),
                channel="cardputer.ask",
                urgency="normal",
            )
            self._start_bridge_prompt_waiter()

        result = await self._await_bridge_prompt_response(timeout_s)
        if result.get("status") != "answered":
            return result

        selected_index = result["selected_index"]
        selected_choice = choices[selected_index] if 0 <= selected_index < len(choices) else ""
        result.update(
            {
                "question": question,
                "selected_choice": selected_choice,
                "choice_count": len(choices),
                "choices": choices,
            }
        )
        return result

    async def confirm_cardputer(self, title: str, detail: str, danger: bool, timeout_s: int) -> dict[str, Any]:
        if timeout_s < 1:
            raise ValueError("Confirmation timeout must be a positive integer.")
        async with self._bridge_prompt_lock:
            self._set_bridge_prompt_with_metadata(
                "bridge_confirmation",
                title or "Confirmation",
                detail,
                options=("Accept", "Reject"),
                channel="cardputer.confirm",
                urgency="high" if danger else "normal",
                danger=danger,
            )
            self._start_bridge_prompt_waiter()

        result = await self._await_bridge_prompt_response(timeout_s)
        if result.get("status") != "answered":
            return result

        approved = bool(result["accepted"])
        result.update(
            {
                "title": title,
                "detail": detail,
                "danger": danger,
                "approved": approved,
                "selected_choice": "Accept" if approved else "Reject",
            }
        )
        return result

    async def dictate_cardputer(self, prompt: str, max_seconds: int) -> dict[str, Any]:
        if max_seconds < 1:
            raise ValueError("max_seconds must be a positive integer.")
        return {
            "status": "unsupported",
            "prompt": prompt,
            "max_seconds": max_seconds,
            "message": "dictate is not implemented yet",
        }

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
        thread_id = await self.transport.start_thread(self.session.workspace_path, self.session.branch)
        self.session.thread_id = thread_id
        self.session.title = self.session.title if self.session.title != "Untitled session" else thread_id

    async def handle_text_prompt(self, text: str) -> list[Event]:
        events = await self._collect_text_prompt_events(text)
        self._notify(events)
        return events

    async def _collect_text_prompt_events(self, text: str) -> list[Event]:
        await self._ensure_thread()
        if self.session.title == "Untitled session" and text.strip():
            self.session.title = text.strip()
        self.session.record_event(Event(EventType.TEXT_PROMPT, {"text": text}))
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
        if (
            not isinstance(self.transport, MockCodexTransport)
            and events
            and events[-1].type == EventType.CODEX_STATUS
            and events[-1].payload.get("kind") == "completed"
        ):
            events.append(
                Event(
                    EventType.CODEX_STATUS,
                    {
                        "kind": "session_status",
                        "content": "idle",
                        "threadId": self.session.thread_id,
                        "status": "done",
                    },
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
            channel = str(message.payload.get("channel") or title)
            urgency = str(message.payload.get("urgency") or "normal")
            return [self._set_bridge_prompt_with_metadata("bridge_notification", title, detail, channel=channel, urgency=urgency)]

        if message.type == CardputerMessageType.BRIDGE_QUESTION:
            title = str(message.payload.get("title") or "Question")
            detail = str(message.payload.get("detail") or "")
            options = tuple(str(option) for option in (message.payload.get("options") or [])[:3])
            return [self._set_bridge_prompt_with_metadata("bridge_question", title, detail, options=options, channel="cardputer.ask")]

        if message.type == CardputerMessageType.BRIDGE_CONFIRMATION:
            title = str(message.payload.get("title") or "Confirmation")
            detail = str(message.payload.get("detail") or "")
            danger = bool(message.payload.get("danger", False))
            return [
                self._set_bridge_prompt_with_metadata(
                    "bridge_confirmation",
                    title,
                    detail,
                    options=("Accept", "Reject"),
                    channel="cardputer.confirm",
                    urgency="high" if danger else "normal",
                    danger=danger,
                )
            ]

        if message.type == CardputerMessageType.BRIDGE_RESPONSE:
            accepted = bool(message.payload.get("accepted", False))
            selected_index = int(message.payload.get("selected_index", 0))
            note = str(message.payload.get("note") or "")
            if self.session.bridge_prompt_kind is None:
                event = Event(EventType.ERROR, {"content": "No bridge prompt is pending."})
                self._notify([event])
                return [event]
            pending_kind = self.session.bridge_prompt_kind
            self._complete_bridge_prompt(accepted, selected_index, note)
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
            sessions = []
            for s in self.session_index.ordered_sessions()[:10]:
                sd = s.to_dict()
                # Prune and limit events to the most recent 5
                if "events" in sd and isinstance(sd["events"], list):
                    pruned_events = []
                    for ev in sd["events"][-5:]:
                        ev_type = ev.get("type", "")
                        ev_payload = ev.get("payload", {})
                        # Only keep what the firmware uses: content/text/message/kind
                        # Note: we collapse it into a simpler structure for the firmware's convenience if possible,
                        # but keeping the existing structure is safer to avoid firmware changes.
                        # The firmware uses: doc["payload"]["content"] | doc["payload"]["text"] | doc["payload"]["message"] | ""
                        # and event_payload["kind"]
                        p = {}
                        for k in ("content", "text", "message", "kind"):
                            if k in ev_payload:
                                p[k] = ev_payload[k]
                        pruned_events.append({"type": ev_type, "payload": p})
                    sd["events"] = pruned_events
                sessions.append(sd)

            runs = [run.to_dict() for run in self.run_index.ordered_runs()[:10]]
            event = Event(
                EventType.CODEX_STATUS,
                {
                    "kind": "session_status",
                    "active_session_id": self.session_index.active_session_id,
                    "active_run_id": self.run_index.active_run_id,
                    "workspace_path": self.session.workspace_path,
                    "branch": self.session.branch,
                    "thread_id": self.session.thread_id,
                    "title": self.session.title,
                    "status": self.session.status,
                    "last_event": self.session.last_event,
                    "state_epoch": self.session.state_epoch,
                    "codex_usage_percent": self.session.codex_usage_percent,
                    "codex_usage_secondary_percent": self.session.codex_usage_secondary_percent,
                    "codex_usage_window_minutes": self.session.codex_usage_window_minutes,
                    "codex_usage_secondary_window_minutes": self.session.codex_usage_secondary_window_minutes,
                    "codex_usage_resets_at": self.session.codex_usage_resets_at,
                    "codex_usage_secondary_resets_at": self.session.codex_usage_secondary_resets_at,
                    "codex_usage_reset_line": self.session.codex_usage_reset_line,
                    "approval_id": self.session.pending_approval_id,
                    "approval_title": self.session.pending_approval_title,
                    "approval_detail": self.session.pending_approval_detail,
                    "sessions": sessions,
                    "runs": runs,
                    "interrupt_supported": True,
                    "bridge_prompt_kind": self.session.bridge_prompt_kind,
                    "bridge_prompt_title": self.session.bridge_prompt_title,
                    "bridge_prompt_detail": self.session.bridge_prompt_detail,
                    "bridge_prompt_channel": self.session.bridge_prompt_channel,
                    "bridge_prompt_urgency": self.session.bridge_prompt_urgency,
                    "bridge_prompt_danger": self.session.bridge_prompt_danger,
                    "bridge_prompt_options": list(self.session.bridge_prompt_options),
                    "bridge_prompt_selected_index": self.session.bridge_prompt_selected_index,
                },
            )
            self._notify([event])
            return [event]

        if message.type == CardputerMessageType.INTERRUPT:
            thread_id = message.payload.get("thread_id") or self.session.thread_id
            if thread_id:
                await self.transport.interrupt_turn(thread_id)
                event = Event(EventType.CODEX_STATUS, {"kind": "status", "content": "interrupt sent", "threadId": thread_id})
                self._notify([event])
                return [event]
            return []

        event = Event(EventType.CODEX_STATUS, {"content": routed.status_line, "kind": "router"})
        self._notify([event])
        return [event]

    def _notify(self, events: list[Event]) -> None:
        self.session_index.record(events)
        self.run_index.record(events, run_id=self.session.run_id)
        if self.event_observer is None or not events:
            return
        self.event_observer(events)
