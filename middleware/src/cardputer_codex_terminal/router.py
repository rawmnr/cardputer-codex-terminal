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
            approval_id = str(message.payload.get("approval_id") or message.payload.get("approvalId") or "")
            note = str(message.payload.get("note") or "")
            return RouterResult(
                status_line="approval accepted" if approved else "approval rejected",
                outbound_events=[
                    {
                        "kind": "approval_response",
                        "approved": approved,
                        "approval_id": approval_id,
                        "note": note,
                    }
                ],
            )

        if message.type == CardputerMessageType.STATUS_REQUEST:
            return RouterResult(status_line="status requested", outbound_events=[{"kind": "status_request"}])

        if message.type == CardputerMessageType.PROJECT_SELECT:
            workspace_path = str(message.payload.get("workspace_path", ""))
            return RouterResult(
                status_line=f"project selected: {workspace_path}",
                outbound_events=[{"kind": "project_select", "workspace_path": workspace_path}],
            )

        if message.type == CardputerMessageType.BRANCH_SELECT:
            branch = str(message.payload.get("branch", ""))
            return RouterResult(
                status_line=f"branch selected: {branch}",
                outbound_events=[{"kind": "branch_select", "branch": branch}],
            )

        if message.type == CardputerMessageType.THREAD_SELECT:
            thread_id = str(message.payload.get("thread_id", ""))
            return RouterResult(
                status_line=f"thread selected: {thread_id}",
                outbound_events=[{"kind": "thread_select", "thread_id": thread_id}],
            )

        return RouterResult(status_line="ping")
