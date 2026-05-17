from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any


def _trim_text(value: str, limit: int = 64) -> str:
    text = value.strip()
    if len(text) <= limit:
        return text
    return text[: limit - 3] + "..."


def _event_payload(event: Any) -> tuple[str, dict[str, Any]]:
    if hasattr(event, "type") and hasattr(event, "payload"):
        return str(event.type), dict(event.payload)
    if isinstance(event, dict):
        return str(event.get("type", "")), dict(event.get("payload", {}))
    return "unknown", {}


@dataclass(slots=True)
class SessionState:
    session_id: str
    thread_id: str | None = None
    workspace_path: str = "."
    branch: str | None = None
    title: str = "Untitled session"
    status: str = "idle"
    last_event: str = ""
    pending_approval_id: str | None = None
    pending_approval_title: str | None = None
    pending_approval_detail: str | None = None
    pending_approval_timeout_seconds: int | None = None
    bridge_prompt_kind: str | None = None
    bridge_prompt_title: str | None = None
    bridge_prompt_detail: str | None = None
    bridge_prompt_channel: str | None = None
    bridge_prompt_urgency: str | None = None
    bridge_prompt_danger: bool | None = None
    bridge_prompt_options: tuple[str, ...] = ()
    bridge_prompt_selected_index: int = 0
    state_epoch: int = 0
    events: list[dict[str, Any]] = field(default_factory=list)

    _max_events = 24

    def touch(self, *, workspace_path: str | None = None, branch: str | None = None, thread_id: str | None = None) -> None:
        if workspace_path is not None and workspace_path:
            self.workspace_path = workspace_path
        if branch is not None:
            self.branch = branch or None
        if thread_id is not None:
            self.thread_id = thread_id or None
        self.state_epoch += 1

    def record_event(self, event: Any) -> None:
        self.state_epoch += 1
        event_type, payload = _event_payload(event)
        event_dict = event.to_dict() if hasattr(event, "to_dict") else event if isinstance(event, dict) else {"type": event_type, "payload": payload}
        if not isinstance(event_dict, dict):
            return

        kind = str(payload.get("kind") or "")
        content = str(payload.get("content") or payload.get("text") or payload.get("message") or "")
        if event_type == "text_prompt":
            if content:
                self.title = _trim_text(content, 40)
                self.last_event = content
            self.status = "running"
        elif event_type == "audio_chunk":
            self.status = "running"
            self.last_event = f"audio chunk {payload.get('chunk_id', '')}".strip()
        elif event_type == "approval_request":
            self.status = "approval"
            approval_id = str(payload.get("approval_id") or payload.get("approvalId") or "")
            self.pending_approval_id = approval_id or None
            self.pending_approval_title = str(payload.get("title") or "") or None
            self.pending_approval_detail = str(payload.get("detail") or "") or None
            timeout_seconds = payload.get("timeout_seconds") or payload.get("timeoutSeconds")
            self.pending_approval_timeout_seconds = int(timeout_seconds) if isinstance(timeout_seconds, int) else None
            if content:
                self.last_event = content
            elif str(payload.get("title") or ""):
                self.last_event = str(payload.get("title"))
        elif event_type == "approval_response":
            self.pending_approval_id = None
            self.pending_approval_title = None
            self.pending_approval_detail = None
            self.pending_approval_timeout_seconds = None
            self.status = "running"
            if content:
                self.last_event = content
            elif str(payload.get("approved")):
                self.last_event = "approval recorded"
        elif event_type == "codex_delta":
            self.status = "running"
            if content:
                self.last_event = content
        elif event_type == "codex_usage":
            if content:
                self.last_event = content
        elif event_type == "codex_status":
            if kind in {"completed", "turn/completed"}:
                self.status = "done"
            elif kind in {"status", "turn/started", "turn/start/accepted", "thread/status/changed"}:
                self.status = "running"
            elif kind == "voice_prompt_transcribed":
                self.status = "running"
            elif kind == "session_status":
                return
            elif kind.startswith("bridge_"):
                return
            if content:
                self.last_event = content
            elif kind:
                self.last_event = kind.replace("_", " ")
        elif event_type == "error":
            self.status = "error"
            if content:
                self.last_event = content
        else:
            if content:
                self.last_event = content

        self.events.append(event_dict)
        if len(self.events) > self._max_events:
            self.events = self.events[-self._max_events :]

    def to_dict(self) -> dict[str, Any]:
        return {
            "session_id": self.session_id,
            "thread_id": self.thread_id,
            "workspace_path": self.workspace_path,
            "branch": self.branch,
            "title": self.title,
            "status": self.status,
            "last_event": self.last_event,
            "state_epoch": self.state_epoch,
            "pending_approval_id": self.pending_approval_id,
            "pending_approval_title": self.pending_approval_title,
            "pending_approval_detail": self.pending_approval_detail,
            "pending_approval_timeout_seconds": self.pending_approval_timeout_seconds,
            "bridge_prompt_kind": self.bridge_prompt_kind,
            "bridge_prompt_title": self.bridge_prompt_title,
            "bridge_prompt_detail": self.bridge_prompt_detail,
            "bridge_prompt_channel": self.bridge_prompt_channel,
            "bridge_prompt_urgency": self.bridge_prompt_urgency,
            "bridge_prompt_danger": self.bridge_prompt_danger,
            "bridge_prompt_options": self.bridge_prompt_options,
            "bridge_prompt_selected_index": self.bridge_prompt_selected_index,
            "events": self.events,
        }


@dataclass(slots=True)
class SessionIndex:
    sessions: dict[str, SessionState] = field(default_factory=dict)
    active_session_id: str | None = None
    _next_session_number: int = 1

    def current(self) -> SessionState | None:
        if self.active_session_id is None:
            return None
        return self.sessions.get(self.active_session_id)

    def select(self, session_id: str) -> SessionState | None:
        if session_id in self.sessions:
            self.active_session_id = session_id
            return self.sessions[session_id]
        return None

    def start_new(self, workspace_path: str = ".", branch: str | None = None) -> SessionState:
        session_id = self._new_session_id()
        session = SessionState(session_id, workspace_path=workspace_path, branch=branch)
        self.sessions[session_id] = session
        self.active_session_id = session_id
        return session

    def ensure_active(self, workspace_path: str = ".", branch: str | None = None, thread_id: str | None = None, title: str | None = None) -> SessionState:
        session = self.current()
        if session is None:
            session = self.start_new(workspace_path, branch)
        if thread_id is not None:
            session.thread_id = thread_id
        if title is not None:
            session.title = title
        return session

    def record(self, events: list[Any]) -> None:
        session = self.current()
        if session is None:
            session = self.start_new()

        for event in events:
            session.record_event(event)

    def ordered_sessions(self) -> list[SessionState]:
        return sorted(self.sessions.values(), key=lambda s: s.session_id, reverse=True)

    def to_dict(self) -> dict[str, Any]:
        sessions = {sid: s.to_dict() for sid, s in self.sessions.items()}
        return {
            "active_session_id": self.active_session_id,
            "sessions": sessions,
        }

    def _new_session_id(self) -> str:
        session_id = f"session-{self._next_session_number:06d}"
        self._next_session_number += 1
        return session_id
