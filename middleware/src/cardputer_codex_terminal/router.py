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

        if message.type == CardputerMessageType.VOICE_PROMPT_READY:
            sample_rate_hz = int(message.payload.get("sample_rate_hz", 16000))
            sample_count = int(message.payload.get("sample_count", 0))
            return RouterResult(
                status_line="voice prompt ready",
                outbound_events=[
                    {
                        "kind": "voice_prompt_ready",
                        "sample_rate_hz": sample_rate_hz,
                        "sample_count": sample_count,
                    }
                ],
            )

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

        if message.type == CardputerMessageType.DISPLAY_SNAPSHOT:
            status_line = str(message.payload.get("status_line") or message.payload.get("content") or "display snapshot")
            event = {
                "kind": "display_snapshot",
                "status_line": status_line,
                "screen_text": str(message.payload.get("screen_text") or ""),
                "active_app": str(message.payload.get("active_app") or ""),
            }
            for key in ("firmware_name", "network_status_line", "input_line"):
                if message.payload.get(key):
                    event[key] = str(message.payload[key])
            return RouterResult(
                status_line=status_line,
                outbound_events=[event],
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

        if message.type == CardputerMessageType.BRIDGE_NOTIFICATION:
            title = str(message.payload.get("title") or "Notification")
            detail = str(message.payload.get("detail") or "")
            return RouterResult(
                status_line=f"bridge notification: {title}",
                outbound_events=[{"kind": "bridge_notification", "title": title, "detail": detail}],
            )

        if message.type == CardputerMessageType.BRIDGE_QUESTION:
            title = str(message.payload.get("title") or "Question")
            detail = str(message.payload.get("detail") or "")
            options = list(message.payload.get("options") or [])
            return RouterResult(
                status_line=f"bridge question: {title}",
                outbound_events=[{"kind": "bridge_question", "title": title, "detail": detail, "options": options}],
            )

        if message.type == CardputerMessageType.BRIDGE_CONFIRMATION:
            title = str(message.payload.get("title") or "Confirmation")
            detail = str(message.payload.get("detail") or "")
            return RouterResult(
                status_line=f"bridge confirmation: {title}",
                outbound_events=[{"kind": "bridge_confirmation", "title": title, "detail": detail}],
            )

        if message.type == CardputerMessageType.BRIDGE_RESPONSE:
            accepted = bool(message.payload.get("accepted", False))
            selected_index = int(message.payload.get("selected_index", 0))
            note = str(message.payload.get("note") or "")
            return RouterResult(
                status_line="bridge response accepted" if accepted else "bridge response rejected",
                outbound_events=[
                    {
                        "kind": "bridge_response",
                        "accepted": accepted,
                        "selected_index": selected_index,
                        "note": note,
                    }
                ],
            )

        return RouterResult(status_line="ping")
