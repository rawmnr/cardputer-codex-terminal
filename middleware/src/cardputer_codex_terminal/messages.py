from __future__ import annotations

from dataclasses import dataclass, field
from enum import StrEnum
from typing import Any


class CardputerMessageType(StrEnum):
    TEXT_PROMPT = "text_prompt"
    AUDIO_CHUNK = "audio_chunk"
    APPROVAL_RESPONSE = "approval_response"
    STATUS_REQUEST = "status_request"
    PING = "ping"


@dataclass(slots=True)
class CardputerMessage:
    type: CardputerMessageType
    payload: dict[str, Any] = field(default_factory=dict)

    def to_dict(self) -> dict[str, Any]:
        return {"type": self.type.value, "payload": self.payload}

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "CardputerMessage":
        if "type" not in data:
            raise ValueError("Missing message type.")
        if not isinstance(data["type"], str):
            raise ValueError("Message type must be a string.")

        message_type = CardputerMessageType(data["type"])
        payload = data.get("payload", {})
        if not isinstance(payload, dict):
            raise ValueError("Message payload must be an object.")
        return cls(message_type, payload)


@dataclass(slots=True)
class RouterResult:
    outbound_events: list[dict[str, Any]] = field(default_factory=list)
    status_line: str = ""

