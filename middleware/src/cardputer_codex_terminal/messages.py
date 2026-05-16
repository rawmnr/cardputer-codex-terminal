from __future__ import annotations

from dataclasses import dataclass, field
from enum import StrEnum
from typing import Any


PROTOCOL_VERSION = 1


class CardputerMessageType(StrEnum):
    TEXT_PROMPT = "text_prompt"
    AUDIO_CHUNK = "audio_chunk"
    APPROVAL_RESPONSE = "approval_response"
    STATUS_REQUEST = "status_request"
    PROJECT_SELECT = "project_select"
    BRANCH_SELECT = "branch_select"
    THREAD_SELECT = "thread_select"
    PING = "ping"


@dataclass(slots=True)
class CardputerMessage:
    type: CardputerMessageType
    payload: dict[str, Any] = field(default_factory=dict)
    protocol_version: int = PROTOCOL_VERSION

    def to_dict(self) -> dict[str, Any]:
        return {
            "protocol_version": self.protocol_version,
            "type": self.type.value,
            "payload": self.payload,
        }

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "CardputerMessage":
        protocol_version = data.get("protocol_version", PROTOCOL_VERSION)
        if not isinstance(protocol_version, int):
            raise ValueError("Protocol version must be an integer.")
        if protocol_version != PROTOCOL_VERSION:
            raise ValueError(f"Unsupported protocol version: {protocol_version}")

        if "type" not in data:
            raise ValueError("Missing message type.")
        if not isinstance(data["type"], str):
            raise ValueError("Message type must be a string.")

        message_type = CardputerMessageType(data["type"])
        payload = data.get("payload", {})
        if not isinstance(payload, dict):
            raise ValueError("Message payload must be an object.")
        return cls(message_type, payload, protocol_version)


@dataclass(slots=True)
class RouterResult:
    outbound_events: list[dict[str, Any]] = field(default_factory=list)
    status_line: str = ""
