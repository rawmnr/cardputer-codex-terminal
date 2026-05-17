from __future__ import annotations

from abc import ABC, abstractmethod
import asyncio
import contextlib
from dataclasses import dataclass, field
import json
import os
import shutil
from typing import AsyncIterator
from typing import Callable, Awaitable, Any


WebSocketFactory = Callable[[str], Awaitable[Any]]


@dataclass(slots=True)
class CodexReply:
    kind: str
    content: str
    data: dict[str, Any] = field(default_factory=dict)


class CodexTransport(ABC):
    @abstractmethod
    async def initialize(self) -> None:
        raise NotImplementedError

    @abstractmethod
    async def start_thread(self, cwd: str, branch: str | None = None) -> str:
        raise NotImplementedError

    @abstractmethod
    async def resume_thread(self, thread_id: str) -> str:
        raise NotImplementedError

    @abstractmethod
    async def update_thread_metadata(self, thread_id: str, branch: str | None = None) -> None:
        raise NotImplementedError

    @abstractmethod
    async def submit_approval(self, approval_id: str, approved: bool, note: str | None = None) -> None:
        raise NotImplementedError

    @abstractmethod
    async def start_turn(self, prompt: str, thread_id: str | None = None, cwd: str | None = None) -> AsyncIterator[CodexReply]:
        raise NotImplementedError


class MockCodexTransport(CodexTransport):
    async def initialize(self) -> None:
        return None

    async def start_thread(self, cwd: str, branch: str | None = None) -> str:
        suffix = branch or "main"
        return f"mock-thread:{cwd}:{suffix}"

    async def resume_thread(self, thread_id: str) -> str:
        return thread_id

    async def update_thread_metadata(self, thread_id: str, branch: str | None = None) -> None:
        return None

    async def submit_approval(self, approval_id: str, approved: bool, note: str | None = None) -> None:
        return None

    async def start_turn(self, prompt: str, thread_id: str | None = None, cwd: str | None = None) -> AsyncIterator[CodexReply]:
        yield CodexReply("status", "mock_codex_ready")
        yield CodexReply("delta", f"Received: {prompt}")
        yield CodexReply("usage", "mock_usage_update", {"threadId": thread_id, "cwd": cwd, "usedPercent": 12})
        yield CodexReply("delta", "This transport is a local placeholder.")
        yield CodexReply("completed", "mock_turn_completed")


class JsonRpcCodexAppServerTransport(CodexTransport):
    def __init__(self) -> None:
        self._initialized = False
        self._next_id = 1
        self._next_approval_id = 1
        self._approval_requests: dict[str, tuple[str, dict[str, Any]]] = {}

    async def initialize(self) -> None:
        await self._ensure_connection()
        await self._request(
            "initialize",
            {
                "clientInfo": {"name": "cardputer-codex-middleware", "version": "0.1.0"},
                "capabilities": {"experimentalApi": True},
            },
        )
        await self._notify("initialized")
        self._initialized = True

    async def start_thread(self, cwd: str, branch: str | None = None) -> str:
        if not self._initialized:
            await self.initialize()

        response = await self._request("thread/start", {"cwd": cwd})
        thread_id = self._extract_thread_id(response)
        if not thread_id:
            raise RuntimeError("Codex thread start did not return a thread id.")

        if branch is not None:
            await self.update_thread_metadata(thread_id, branch=branch)

        return thread_id

    async def resume_thread(self, thread_id: str) -> str:
        if not self._initialized:
            await self.initialize()

        response = await self._request("thread/resume", {"threadId": thread_id})
        resumed_thread_id = self._extract_thread_id(response)
        if not resumed_thread_id:
            raise RuntimeError("Codex thread resume did not return a thread id.")
        return resumed_thread_id

    async def update_thread_metadata(self, thread_id: str, branch: str | None = None) -> None:
        if branch is None:
            return None

        if not self._initialized:
            await self.initialize()

        await self._request(
            "thread/metadata/update",
            {
                "threadId": thread_id,
                "gitInfo": {"branch": branch},
            },
        )
        return None

    async def submit_approval(self, approval_id: str, approved: bool, note: str | None = None) -> None:
        if not self._initialized:
            await self.initialize()

        await self._send_response(approval_id, self._approval_response_result(approval_id, approved, note))
        return None

    async def start_turn(self, prompt: str, thread_id: str | None = None, cwd: str | None = None) -> AsyncIterator[CodexReply]:
        if not self._initialized:
            await self.initialize()
        if thread_id is None:
            raise ValueError("Codex app-server turn/start requires a threadId.")

        params: dict[str, Any] = {"threadId": thread_id, "input": [{"type": "text", "text": prompt}]}
        if cwd is not None:
            params["cwd"] = cwd

        request_id = self._new_id("turn-start")
        await self._send_payload({"id": request_id, "method": "turn/start", "params": params})

        while True:
            message = await self._read_payload()
            if message.get("id") == request_id:
                self._response_result(message, "turn/start")
                continue
            else:
                reply = self._decode_reply(message)
            if reply is None:
                continue
            yield reply
            if reply.kind == "completed":
                return

    async def _request(self, method: str, params: dict[str, Any] | None = None) -> dict[str, Any]:
        request_id = self._new_id(method)
        payload: dict[str, Any] = {"id": request_id, "method": method}
        if params is not None:
            payload["params"] = params
        await self._send_payload(payload)

        while True:
            message = await self._read_payload()
            if message.get("id") == request_id:
                return self._response_result(message, method)

    async def _notify(self, method: str, params: dict[str, Any] | None = None) -> None:
        payload: dict[str, Any] = {"method": method}
        if params is not None:
            payload["params"] = params
        await self._send_payload(payload)

    async def _send_response(self, request_id: str, result: dict[str, Any]) -> None:
        await self._send_payload({"id": request_id, "result": result})

    def _response_result(self, message: dict[str, Any], method: str) -> dict[str, Any]:
        if message.get("error") is not None:
            raise RuntimeError(f"Codex {method} failed: {message['error']}")
        result = message.get("result", {})
        if not isinstance(result, dict):
            raise RuntimeError(f"Codex {method} response result must be an object.")
        return result

    def _new_id(self, prefix: str) -> str:
        request_id = f"{prefix}-{self._next_id}"
        self._next_id += 1
        return request_id

    def _new_approval_id(self, prefix: str) -> str:
        approval_id = f"{prefix}-approval-{self._next_approval_id}"
        self._next_approval_id += 1
        return approval_id

    @abstractmethod
    async def _ensure_connection(self) -> None:
        raise NotImplementedError

    @abstractmethod
    async def _send_payload(self, payload: dict[str, Any]) -> None:
        raise NotImplementedError

    @abstractmethod
    async def _read_payload(self) -> dict[str, Any]:
        raise NotImplementedError

    def _decode_reply(self, message: dict[str, Any]) -> CodexReply | None:
        method = message.get("method")
        params = message.get("params") if isinstance(message.get("params"), dict) else {}
        if isinstance(method, str):
            return self._decode_json_rpc_message(method, params, message)

        kind = message.get("kind") or message.get("type") or message.get("event")
        content = message.get("content") or message.get("text") or message.get("message") or ""
        data = message.get("data")
        if not isinstance(data, dict):
            data = {}

        if kind in {"status", "turn/status"}:
            return CodexReply("status", str(content), data)
        if kind in {"delta", "item/agentMessage/delta"}:
            return CodexReply("delta", str(content), data)
        if kind in {"approval_request", "approval/requested", "approval/request"} or (
            "approval" in str(kind) and "request" in str(kind)
        ):
            return CodexReply("approval_request", str(content or "approval requested"), self._format_approval_request_data(message))
        if kind in {"usage", "thread/tokenUsage/updated", "account/rateLimits/updated"}:
            return CodexReply("usage", str(content or self._format_usage_content(message)), data or message)
        if kind in {"completed", "turn/completed"}:
            return CodexReply("completed", str(content or "turn_completed"), data)
        if kind in {"error", "turn/error"}:
            return CodexReply("status", f"error: {content}", data)
        return None

    def _decode_json_rpc_message(
        self, method: str, params: dict[str, Any], message: dict[str, Any]
    ) -> CodexReply | None:
        if method in {"turn/start/accepted", "turn/started", "thread/status/changed"}:
            return CodexReply("status", method, params)
        if method == "item/agentMessage/delta":
            delta = params.get("delta") or params.get("text") or params.get("content") or ""
            return CodexReply("delta", str(delta), params)
        if method in {"item/reasoning/summaryTextDelta", "item/reasoning/textDelta", "item/plan/delta"}:
            delta = params.get("delta") or params.get("text") or ""
            return CodexReply("delta", str(delta), params)
        if method == "thread/tokenUsage/updated":
            return CodexReply("usage", self._format_usage_content(params), {"method": method, **params})
        if method == "account/rateLimits/updated":
            return CodexReply("usage", self._format_usage_content(params), {"method": method, **params})
        if method in {
            "item/commandExecution/requestApproval",
            "item/fileChange/requestApproval",
            "item/permissions/requestApproval",
            "execCommandApproval",
            "applyPatchApproval",
        }:
            data = dict(params)
            approval_id = str(message.get("id") or params.get("approvalId") or "")
            if not approval_id:
                approval_id = self._new_approval_id(method)
            data["approval_id"] = approval_id
            data["server_request_method"] = method
            self._approval_requests[data["approval_id"]] = (method, dict(params))
            return CodexReply("approval_request", "approval requested", data)
        if method == "turn/completed":
            turn = params.get("turn")
            status = turn.get("status") if isinstance(turn, dict) else None
            content = "turn_completed" if status in {None, "completed"} else f"turn_{status}"
            return CodexReply("completed", content, params)
        if method == "error":
            content = params.get("message") or params.get("error") or "codex app-server error"
            return CodexReply("status", f"error: {content}", params)
        return None

    def _approval_response_result(self, approval_id: str, approved: bool, note: str | None = None) -> dict[str, Any]:
        method, params = self._approval_requests.pop(approval_id, ("item/commandExecution/requestApproval", {}))
        if method == "item/permissions/requestApproval":
            return {
                "permissions": params.get("permissions", {}) if approved else {},
                "scope": "turn",
            }
        if method in {"execCommandApproval", "applyPatchApproval"}:
            return {"decision": "approved" if approved else "denied"}
        return {"decision": "accept" if approved else "decline"}

    def _format_usage_content(self, message: dict[str, Any]) -> str:
        if "primary" in message or "rateLimits" in message:
            rate_limits = message.get("rateLimits") or message
            if isinstance(rate_limits, dict):
                primary = rate_limits.get("primary")
                if isinstance(primary, dict):
                    used = primary.get("usedPercent")
                    window = primary.get("windowDurationMins")
                    reset = primary.get("resetsAt")
                    return f"rate limit used={used}% window={window}m reset={reset}"
        token_usage = message.get("tokenUsage") if isinstance(message.get("tokenUsage"), dict) else message
        if isinstance(token_usage, dict):
            total = token_usage.get("total")
            if isinstance(total, dict):
                input_tokens = total.get("inputTokens")
                output_tokens = total.get("outputTokens")
                cached_tokens = total.get("cachedInputTokens")
                reasoning_tokens = total.get("reasoningOutputTokens")
                return (
                    f"thread usage input={input_tokens} output={output_tokens} "
                    f"cached={cached_tokens} reasoning={reasoning_tokens}"
                )
            if "inputTokens" in token_usage or "outputTokens" in token_usage:
                input_tokens = token_usage.get("inputTokens")
                output_tokens = token_usage.get("outputTokens")
                cached_tokens = token_usage.get("cachedInputTokens")
                reasoning_tokens = token_usage.get("reasoningTokens") or token_usage.get("reasoningOutputTokens")
                return (
                    f"thread usage input={input_tokens} output={output_tokens} "
                    f"cached={cached_tokens} reasoning={reasoning_tokens}"
                )
        return "usage update"

    def _extract_thread_id(self, message: dict[str, Any]) -> str | None:
        thread = message.get("thread")
        if isinstance(thread, dict):
            thread_id = thread.get("id")
            if isinstance(thread_id, str) and thread_id:
                return thread_id

        params = message.get("params")
        if isinstance(params, dict):
            thread = params.get("thread")
            if isinstance(thread, dict):
                thread_id = thread.get("id")
                if isinstance(thread_id, str) and thread_id:
                    return thread_id

        thread_id = message.get("threadId")
        if isinstance(thread_id, str) and thread_id:
            return thread_id
        return None

    def _format_approval_request_data(self, message: dict[str, Any]) -> dict[str, Any]:
        payload = {
            key: value
            for key, value in message.items()
            if key not in {"kind", "type", "event", "content", "text", "message"}
        }
        approval_data = payload.get("data")
        if isinstance(approval_data, dict):
            return approval_data
        return payload


class StdioCodexAppServerTransport(JsonRpcCodexAppServerTransport):
    # Codex app-server can emit large JSON payloads, especially for streamed
    # deltas and initial session snapshots. The asyncio default line limit is
    # too small for that output, so we raise it for the subprocess pipes.
    _stream_limit = 1024 * 1024

    def __init__(self, command: list[str] | None = None, cwd: str | None = None) -> None:
        super().__init__()
        self.command = command or ["codex", "app-server"]
        self.cwd = cwd
        self._process: asyncio.subprocess.Process | None = None
        self._stderr_task: asyncio.Task[None] | None = None
        self._stderr_tail: list[str] = []

    async def close(self) -> None:
        if self._process is not None:
            if self._process.stdin is not None:
                self._process.stdin.close()
                with contextlib.suppress(Exception):
                    await self._process.stdin.wait_closed()
            try:
                await asyncio.wait_for(self._process.wait(), timeout=2)
            except asyncio.TimeoutError:
                self._process.terminate()
                try:
                    await asyncio.wait_for(self._process.wait(), timeout=5)
                except asyncio.TimeoutError:
                    self._process.kill()
                    await self._process.wait()
        self._process = None
        if self._stderr_task is not None:
            self._stderr_task.cancel()
            with contextlib.suppress(asyncio.CancelledError):
                await self._stderr_task
            self._stderr_task = None
        self._initialized = False

    async def _ensure_connection(self) -> None:
        if self._process is not None:
            return
        command = self._resolve_command(self.command)
        self._process = await asyncio.create_subprocess_exec(
            *command,
            stdin=asyncio.subprocess.PIPE,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
            cwd=self.cwd,
            limit=self._stream_limit,
        )
        self._stderr_task = asyncio.create_task(self._drain_stderr())

    async def _send_payload(self, payload: dict[str, Any]) -> None:
        await self._ensure_connection()
        if self._process is None or self._process.stdin is None:
            raise RuntimeError("Codex app-server stdio is not writable.")
        self._process.stdin.write(json.dumps(payload, ensure_ascii=False).encode("utf-8") + b"\n")
        await self._process.stdin.drain()

    async def _read_payload(self) -> dict[str, Any]:
        await self._ensure_connection()
        if self._process is None or self._process.stdout is None:
            raise RuntimeError("Codex app-server stdio is not readable.")
        raw = await self._process.stdout.readline()
        if not raw:
            stderr = "\n".join(self._stderr_tail).strip()
            detail = f": {stderr}" if stderr else ""
            raise RuntimeError(f"Codex app-server exited before sending a response{detail}")
        data = json.loads(raw.decode("utf-8"))
        if not isinstance(data, dict):
            raise TypeError("Codex app-server stdio messages must be JSON objects.")
        return data

    async def _drain_stderr(self) -> None:
        if self._process is None or self._process.stderr is None:
            return
        while True:
            raw = await self._process.stderr.readline()
            if not raw:
                return
            self._stderr_tail.append(raw.decode("utf-8", errors="replace").rstrip())
            self._stderr_tail = self._stderr_tail[-20:]

    def _resolve_command(self, command: list[str]) -> list[str]:
        if not command:
            raise ValueError("Codex app-server command cannot be empty.")
        executable = command[0]
        if os.name == "nt":
            resolved = shutil.which(executable) or executable
            resolved_lower = resolved.lower()
            if resolved_lower.endswith((".bat", ".cmd")):
                cmd = os.environ.get("COMSPEC") or shutil.which("cmd") or "cmd.exe"
                return [cmd, "/d", "/c", resolved, *command[1:]]
            if resolved_lower.endswith(".ps1"):
                powershell = shutil.which("pwsh") or shutil.which("powershell")
                if powershell is not None:
                    return [powershell, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", resolved, *command[1:]]
            if resolved != executable:
                return [resolved, *command[1:]]
        return command


class LocalWebSocketCodexTransport(JsonRpcCodexAppServerTransport):
    def __init__(self, ws_url: str, connect_factory: WebSocketFactory | None = None) -> None:
        super().__init__()
        self.ws_url = ws_url
        self._connect_factory = connect_factory
        self._ws: Any | None = None

    async def close(self) -> None:
        if self._ws is not None:
            close = getattr(self._ws, "close", None)
            if callable(close):
                result = close()
                if hasattr(result, "__await__"):
                    await result
        self._ws = None
        self._initialized = False

    async def _ensure_connection(self) -> None:
        if self._ws is not None:
            return None

        if self._connect_factory is not None:
            self._ws = await self._connect_factory(self.ws_url)
            return None

        import websockets  # type: ignore

        self._ws = await websockets.connect(self.ws_url)
        return None

    async def _send_payload(self, payload: dict[str, Any]) -> None:
        await self._ensure_connection()
        if self._ws is None:
            raise RuntimeError("Codex app-server WebSocket is not connected.")
        await self._ws.send(json.dumps(payload, ensure_ascii=False))

    async def _read_payload(self) -> dict[str, Any]:
        await self._ensure_connection()
        if self._ws is None:
            raise RuntimeError("Codex app-server WebSocket is not connected.")
        raw = await self._ws.recv()
        if isinstance(raw, bytes):
            raw = raw.decode("utf-8")
        if not isinstance(raw, str):
            raise TypeError("WebSocket messages must decode to text JSON.")
        data = json.loads(raw)
        if not isinstance(data, dict):
            raise TypeError("WebSocket JSON messages must be objects.")
        return data
