from __future__ import annotations

from dataclasses import dataclass, field
from enum import StrEnum
from typing import Any


class EventType(StrEnum):
    TEXT_PROMPT = "text_prompt"
    AUDIO_CHUNK = "audio_chunk"
    APPROVAL_REQUEST = "approval_request"
    APPROVAL_RESPONSE = "approval_response"
    CODEX_DELTA = "codex_delta"
    CODEX_STATUS = "codex_status"
    ERROR = "error"


@dataclass(slots=True)
class Event:
    type: EventType
    payload: dict[str, Any] = field(default_factory=dict)

    def to_dict(self) -> dict[str, Any]:
        return {"type": self.type.value, "payload": self.payload}


@dataclass(slots=True)
class TextPrompt(Event):
    text: str = ""

    def __init__(self, text: str) -> None:
        super().__init__(EventType.TEXT_PROMPT, {"text": text})
        self.text = text


@dataclass(slots=True)
class AudioChunk(Event):
    chunk_id: int = 0
    pcm_b64: str = ""

    def __init__(self, chunk_id: int, pcm_b64: str) -> None:
        super().__init__(EventType.AUDIO_CHUNK, {"chunk_id": chunk_id, "pcm_b64": pcm_b64})
        self.chunk_id = chunk_id
        self.pcm_b64 = pcm_b64
