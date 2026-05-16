from __future__ import annotations

from dataclasses import dataclass

from .messages import CardputerMessage, CardputerMessageType, RouterResult


@dataclass(slots=True)
class CardputerRouter:
    def route(self, message: CardputerMessage) -> RouterResult:
        if message.type == CardputerMessageType.TEXT_PROMPT:
            text = str(message.payload.get("text", ""))
            return RouterResult(status_line="text prompt received", outbound_events=[{"kind": "text_prompt", "text": text}])

        if message.type == CardputerMessageType.AUDIO_CHUNK:
            chunk_id = int(message.payload.get("chunk_id", 0))
            return RouterResult(status_line="audio chunk queued", outbound_events=[{"kind": "audio_chunk", "chunk_id": chunk_id}])

        if message.type == CardputerMessageType.APPROVAL_RESPONSE:
            approved = bool(message.payload.get("approved", False))
            return RouterResult(
                status_line="approval accepted" if approved else "approval rejected",
                outbound_events=[{"kind": "approval_response", "approved": approved}],
            )

        if message.type == CardputerMessageType.STATUS_REQUEST:
            return RouterResult(status_line="status requested", outbound_events=[{"kind": "status_request"}])

        return RouterResult(status_line="ping")

