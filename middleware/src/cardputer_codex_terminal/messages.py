from __future__ import annotations

from dataclasses import dataclass, field
from enum import StrEnum
from itertools import count
from typing import Any


PROTOCOL_VERSION = 2


class CardputerMessageType(StrEnum):
    HELLO = "hello"
    HELLO_ACK = "hello_ack"
    TEXT_PROMPT = "text_prompt"
    AUDIO_CHUNK = "audio_chunk"
    VOICE_PROMPT_READY = "voice_prompt_ready"
    APPROVAL_RESPONSE = "approval_response"
    DISPLAY_SNAPSHOT = "display_snapshot"
    STATUS_REQUEST = "status_request"
    PROJECT_SELECT = "project_select"
    BRANCH_SELECT = "branch_select"
    THREAD_SELECT = "thread_select"
    BRIDGE_NOTIFICATION = "bridge_notification"
    BRIDGE_QUESTION = "bridge_question"
    BRIDGE_CONFIRMATION = "bridge_confirmation"
    BRIDGE_RESPONSE = "bridge_response"
    INTERRUPT = "interrupt"
    RUN_LIST_REQUEST = "run_list_request"
    RUN_DETAIL_REQUEST = "run_detail_request"
    APPROVAL_INBOX_REQUEST = "approval_inbox_request"
    PING = "ping"


@dataclass(slots=True)
class CardputerMessage:
    type: CardputerMessageType
    payload: dict[str, Any] = field(default_factory=dict)
    protocol_version: int = PROTOCOL_VERSION
    auth_token: str | None = None
    id: str = ""

    _next_id = count(1)

    def to_dict(self) -> dict[str, Any]:
        if not self.id:
            self.id = f"msg-{next(self._next_id):06d}"
        data = {
            "protocol_version": self.protocol_version,
            "id": self.id,
            "type": self.type.value,
            "payload": self.payload,
        }
        if self.auth_token is not None:
            data["auth_token"] = self.auth_token
        return data

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "CardputerMessage":
        protocol_version = data.get("protocol_version", 1)
        if not isinstance(protocol_version, int):
            raise ValueError("Protocol version must be an integer.")
        if protocol_version > PROTOCOL_VERSION:
            raise ValueError(f"Unsupported protocol version: {protocol_version}")

        if "id" not in data:
            raise ValueError("Missing message id.")
        if not isinstance(data["id"], str) or not data["id"]:
            raise ValueError("Message id must be a non-empty string.")

        if "type" not in data:
            raise ValueError("Missing message type.")
        if not isinstance(data["type"], str):
            raise ValueError("Message type must be a string.")

        message_type = CardputerMessageType(data["type"])
        payload = data.get("payload", {})
        if not isinstance(payload, dict):
            raise ValueError("Message payload must be an object.")
        auth_token = data.get("auth_token")
        if auth_token is not None and not isinstance(auth_token, str):
            raise ValueError("Auth token must be a string when provided.")
        return cls(message_type, payload, protocol_version, auth_token if auth_token else None, str(data["id"]))


@dataclass(slots=True)
class RouterResult:
    outbound_events: list[dict[str, Any]] = field(default_factory=list)
    status_line: str = ""
