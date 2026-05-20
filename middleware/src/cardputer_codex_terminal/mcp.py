from __future__ import annotations

import asyncio
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, AsyncIterator

from .core import MiddlewareApp
from .lvgl_preview import run_lvgl_preview


SUPPORTED_PROTOCOL_VERSIONS = ("2025-11-25", "2025-06-18", "2025-03-26", "2024-11-05")


def _text_content(text: str) -> dict[str, Any]:
    return {"type": "text", "text": text}


def _tool_result(structured: dict[str, Any], *, is_error: bool = False, message: str | None = None) -> dict[str, Any]:
    content_text = message or json.dumps(structured, ensure_ascii=False)
    result: dict[str, Any] = {
        "content": [_text_content(content_text)],
        "structuredContent": structured,
    }
    if is_error:
        result["isError"] = True
    return result


@dataclass(slots=True)
class CardputerMcpServer:
    app: MiddlewareApp
    protocol_version: str | None = None
    initialized: bool = False

    def tool_definitions(self) -> list[dict[str, Any]]:
        return [
            {
                "name": "cardputer.notify",
                "title": "Notify Cardputer",
                "description": "Show a notification on the physical Cardputer without waiting for a response.",
                "inputSchema": {
                    "type": "object",
                    "additionalProperties": False,
                    "properties": {
                        "title": {"type": "string"},
                        "body": {"type": "string"},
                        "urgency": {
                            "type": "string",
                            "enum": ["low", "normal", "high", "critical"],
                        },
                    },
                    "required": ["title", "body", "urgency"],
                },
                "outputSchema": {
                    "type": "object",
                    "additionalProperties": False,
                    "properties": {
                        "status": {"type": "string"},
                        "title": {"type": "string"},
                        "body": {"type": "string"},
                        "urgency": {"type": "string"},
                    },
                    "required": ["status", "title", "body", "urgency"],
                },
                "annotations": {
                    "readOnlyHint": True,
                    "idempotentHint": True,
                    "openWorldHint": False,
                    "title": "Notify Cardputer",
                },
            },
            {
                "name": "cardputer.ask",
                "title": "Ask Cardputer",
                "description": "Ask the human on the Cardputer to choose one option and wait for a physical response.",
                "inputSchema": {
                    "type": "object",
                    "additionalProperties": False,
                    "properties": {
                        "question": {"type": "string"},
                        "choices": {
                            "type": "array",
                            "items": {"type": "string"},
                            "minItems": 1,
                            "maxItems": 3,
                        },
                        "timeout_s": {"type": "integer", "minimum": 1},
                    },
                    "required": ["question", "choices", "timeout_s"],
                },
                "outputSchema": {
                    "type": "object",
                    "additionalProperties": False,
                    "properties": {
                    "status": {"type": "string"},
                    "timed_out": {"type": "boolean"},
                    "selected_index": {"type": ["integer", "null"]},
                    "selected_choice": {"type": "string"},
                    "question": {"type": "string"},
                    "choices": {"type": "array", "items": {"type": "string"}},
                    },
                    "required": ["status", "timed_out", "selected_index", "selected_choice", "question", "choices"],
                },
                "annotations": {
                    "readOnlyHint": False,
                    "destructiveHint": False,
                    "openWorldHint": True,
                    "title": "Ask Cardputer",
                },
            },
            {
                "name": "cardputer.confirm",
                "title": "Confirm on Cardputer",
                "description": "Ask for a destructive-action confirmation on the physical Cardputer and wait for accept or reject.",
                "inputSchema": {
                    "type": "object",
                    "additionalProperties": False,
                    "properties": {
                        "title": {"type": "string"},
                        "detail": {"type": "string"},
                        "danger": {"type": "boolean"},
                        "timeout_s": {"type": "integer", "minimum": 1},
                    },
                    "required": ["title", "detail", "danger", "timeout_s"],
                },
                "outputSchema": {
                    "type": "object",
                    "additionalProperties": False,
                    "properties": {
                        "status": {"type": "string"},
                        "timed_out": {"type": "boolean"},
                        "approved": {"type": "boolean"},
                        "selected_choice": {"type": "string"},
                        "title": {"type": "string"},
                        "detail": {"type": "string"},
                        "danger": {"type": "boolean"},
                    },
                    "required": ["status", "timed_out", "approved", "selected_choice", "title", "detail", "danger"],
                },
                "annotations": {
                    "readOnlyHint": False,
                    "destructiveHint": True,
                    "openWorldHint": True,
                    "title": "Confirm on Cardputer",
                },
            },
            {
                "name": "cardputer.show",
                "title": "Show on Cardputer",
                "description": "Display text on a named Cardputer channel without waiting for a response.",
                "inputSchema": {
                    "type": "object",
                    "additionalProperties": False,
                    "properties": {
                        "text": {"type": "string"},
                        "channel": {"type": "string"},
                    },
                    "required": ["text", "channel"],
                },
                "outputSchema": {
                    "type": "object",
                    "additionalProperties": False,
                    "properties": {
                        "status": {"type": "string"},
                        "text": {"type": "string"},
                        "channel": {"type": "string"},
                    },
                    "required": ["status", "text", "channel"],
                },
                "annotations": {
                    "readOnlyHint": True,
                    "idempotentHint": True,
                    "openWorldHint": False,
                    "title": "Show on Cardputer",
                },
            },
            {
                "name": "cardputer.dictate",
                "title": "Dictate from Cardputer",
                "description": "Reserved for later voice capture support.",
                "inputSchema": {
                    "type": "object",
                    "additionalProperties": False,
                    "properties": {
                        "prompt": {"type": "string"},
                        "max_seconds": {"type": "integer", "minimum": 1},
                    },
                    "required": ["prompt", "max_seconds"],
                },
                "outputSchema": {
                    "type": "object",
                    "additionalProperties": False,
                    "properties": {
                        "status": {"type": "string"},
                        "prompt": {"type": "string"},
                        "max_seconds": {"type": "integer"},
                        "message": {"type": "string"},
                    },
                    "required": ["status", "prompt", "max_seconds", "message"],
                },
                "annotations": {
                    "readOnlyHint": True,
                    "idempotentHint": True,
                    "openWorldHint": False,
                    "title": "Dictate from Cardputer",
                },
            },
            {
                "name": "cardputer.preview_lvgl_ui",
                "title": "Preview LVGL UI",
                "description": "Generate one or more 240x135 PNG previews of a Cardputer LVGL screen using a fixture and optional navigation actions.",
                "inputSchema": {
                    "type": "object",
                    "additionalProperties": False,
                    "properties": {
                        "screen": {
                            "type": "string",
                            "enum": ["buddy", "push", "pager", "usage", "bridge", "settings"],
                        },
                        "fixture": {
                            "oneOf": [
                                {"type": "string"},
                                {"type": "object"},
                            ]
                        },
                        "actions": {
                            "type": "array",
                            "items": {"type": "string"},
                        },
                    },
                    "required": ["screen"],
                },
                "outputSchema": {
                    "type": "object",
                    "additionalProperties": False,
                    "properties": {
                        "status": {"type": "string"},
                        "image_b64": {"type": "string"},
                        "frame_count": {"type": "integer"},
                        "frames": {
                            "type": "array",
                            "items": {
                                "type": "object",
                                "additionalProperties": False,
                                "properties": {
                                    "index": {"type": "integer"},
                                    "label": {"type": "string"},
                                    "filename": {"type": "string"},
                                    "width": {"type": "integer"},
                                    "height": {"type": "integer"},
                                },
                                "required": ["index", "label", "filename", "width", "height"],
                            },
                        },
                        "width": {"type": "integer"},
                        "height": {"type": "integer"},
                        "fixture": {"type": "object"},
                    },
                    "required": ["status", "image_b64", "frame_count", "frames", "width", "height"],
                },
            },
        ]

    def _initialize_result(self, protocol_version: str) -> dict[str, Any]:
        return {
            "protocolVersion": protocol_version,
            "capabilities": {
                "tools": {"listChanged": False},
            },
            "serverInfo": {
                "name": "cardputer-codex-middleware",
                "version": "0.1.0",
            },
            "instructions": "Use the cardputer.* tools to route human-visible notifications and approvals to the physical Cardputer.",
        }

    def _resolve_protocol_version(self, requested: str) -> str:
        if requested in SUPPORTED_PROTOCOL_VERSIONS:
            return requested
        return SUPPORTED_PROTOCOL_VERSIONS[0]

    async def handle_message(self, message: dict[str, Any]) -> dict[str, Any] | None:
        method = message.get("method")
        request_id = message.get("id")

        if method is None:
            return None

        if method == "notifications/initialized":
            self.initialized = True
            return None

        if request_id is None:
            return None

        if method == "initialize":
            params = message.get("params") or {}
            protocol_version = self._resolve_protocol_version(str(params.get("protocolVersion") or SUPPORTED_PROTOCOL_VERSIONS[0]))
            self.protocol_version = protocol_version
            return {"jsonrpc": "2.0", "id": request_id, "result": self._initialize_result(protocol_version)}

        if not self.initialized and method not in {"ping"}:
            return self._error(request_id, -32002, "MCP server is not initialized yet.")

        if method == "ping":
            return {"jsonrpc": "2.0", "id": request_id, "result": {}}

        if method == "tools/list":
            return {
                "jsonrpc": "2.0",
                "id": request_id,
                "result": {
                    "tools": self.tool_definitions(),
                },
            }

        if method == "tools/call":
            try:
                result = await self._call_tool(message.get("params") or {})
            except ValueError as exc:
                return self._error(request_id, -32602, str(exc))
            except RuntimeError as exc:
                return {
                    "jsonrpc": "2.0",
                    "id": request_id,
                    "result": _tool_result({"status": "error", "message": str(exc)}, is_error=True, message=str(exc)),
                }
            return {"jsonrpc": "2.0", "id": request_id, "result": result}

        return self._error(request_id, -32601, f"Unknown method: {method}")

    async def _call_tool(self, params: dict[str, Any]) -> dict[str, Any]:
        if not isinstance(params, dict):
            raise ValueError("Tool call parameters must be an object.")

        name = params.get("name")
        if not isinstance(name, str) or not name:
            raise ValueError("Tool name must be a non-empty string.")

        arguments = params.get("arguments") or {}
        if not isinstance(arguments, dict):
            raise ValueError("Tool arguments must be an object.")

        if name == "cardputer.notify":
            title = str(arguments.get("title") or "")
            body = str(arguments.get("body") or "")
            urgency = str(arguments.get("urgency") or "normal")
            if urgency not in {"low", "normal", "high", "critical"}:
                raise ValueError("Urgency must be one of low, normal, high, or critical.")
            await self.app.notify_cardputer(title, body, urgency)
            structured = {"status": "delivered", "title": title, "body": body, "urgency": urgency}
            return _tool_result(structured)

        if name == "cardputer.show":
            text = str(arguments.get("text") or "")
            channel = str(arguments.get("channel") or "")
            if not channel:
                raise ValueError("Channel must be a non-empty string.")
            await self.app.show_cardputer(text, channel)
            structured = {"status": "displayed", "text": text, "channel": channel}
            return _tool_result(structured)

        if name == "cardputer.ask":
            question = str(arguments.get("question") or "")
            choices = arguments.get("choices")
            timeout_s = arguments.get("timeout_s")
            if not question:
                raise ValueError("Question must be a non-empty string.")
            if not isinstance(choices, list) or not all(isinstance(choice, str) for choice in choices):
                raise ValueError("Choices must be an array of strings.")
            if not isinstance(timeout_s, int) or timeout_s < 1:
                raise ValueError("Timeout must be a positive integer.")
            result = await self.app.ask_cardputer(question, list(choices), timeout_s)
            structured = {
                "status": result.get("status", "answered"),
                "timed_out": bool(result.get("timed_out", False)),
                "selected_index": result.get("selected_index"),
                "selected_choice": str(result.get("selected_choice") or ""),
                "question": question,
                "choices": list(choices),
            }
            if result.get("note") is not None:
                structured["note"] = result.get("note")
            return _tool_result(structured, is_error=structured["status"] == "timeout")

        if name == "cardputer.confirm":
            title = str(arguments.get("title") or "")
            detail = str(arguments.get("detail") or "")
            danger = bool(arguments.get("danger", False))
            timeout_s = arguments.get("timeout_s")
            if not title:
                raise ValueError("Title must be a non-empty string.")
            if not isinstance(timeout_s, int) or timeout_s < 1:
                raise ValueError("Timeout must be a positive integer.")
            result = await self.app.confirm_cardputer(title, detail, danger, timeout_s)
            structured = {
                "status": result.get("status", "answered"),
                "timed_out": bool(result.get("timed_out", False)),
                "approved": bool(result.get("approved", False)),
                "selected_choice": str(result.get("selected_choice") or ""),
                "title": title,
                "detail": detail,
                "danger": danger,
            }
            if result.get("note") is not None:
                structured["note"] = result.get("note")
            return _tool_result(structured, is_error=structured["status"] == "timeout")

        if name == "cardputer.dictate":
            prompt = str(arguments.get("prompt") or "")
            max_seconds = arguments.get("max_seconds")
            if not prompt:
                raise ValueError("Prompt must be a non-empty string.")
            if not isinstance(max_seconds, int) or max_seconds < 1:
                raise ValueError("max_seconds must be a positive integer.")
            result = await self.app.dictate_cardputer(prompt, max_seconds)
            return _tool_result(result, is_error=result.get("status") == "unsupported")

        if name == "cardputer.preview_lvgl_ui":
            screen = str(arguments.get("screen") or "buddy")
            fixture = arguments.get("fixture")
            actions = arguments.get("actions")
            if not isinstance(actions, list) and actions is not None:
                raise ValueError("Actions must be an array of strings.")

            workspace_root = Path(__file__).parent.parent.parent.parent.resolve()

            try:
                result = await asyncio.to_thread(
                    run_lvgl_preview,
                    workspace_root=workspace_root,
                    screen=screen,
                    fixture=fixture,
                    actions=actions,
                )
                structured_frames = [
                    {
                        "index": frame["index"],
                        "label": frame["label"],
                        "filename": frame["filename"],
                        "width": frame["width"],
                        "height": frame["height"],
                    }
                    for frame in result["frames"]
                ]
                structured = {
                    "status": "success",
                    "image_b64": result["image_b64"],
                    "frame_count": result["frame_count"],
                    "frames": structured_frames,
                    "width": result["width"],
                    "height": result["height"],
                    "fixture": result["fixture"],
                }

                mcp_result = _tool_result(structured)
                for frame in result["frames"]:
                    mcp_result["content"].append(
                        {
                            "type": "image",
                            "data": frame["image_b64"],
                            "mimeType": "image/png",
                        }
                    )
                return mcp_result
            except Exception as exc:
                raise RuntimeError(f"LVGL preview failed: {exc}")

        raise ValueError(f"Unknown tool: {name}")

    def _error(self, request_id: Any, code: int, message: str) -> dict[str, Any]:
        return {
            "jsonrpc": "2.0",
            "id": request_id,
            "error": {
                "code": code,
                "message": message,
            },
        }

    async def run_stdio(self, stdin: Any = None, stdout: Any = None) -> None:
        stdin = stdin or sys.stdin
        stdout = stdout or sys.stdout

        while True:
            raw = await asyncio.to_thread(stdin.readline)
            if raw == "":
                return

            line = raw.strip()
            if not line:
                continue

            try:
                message = json.loads(line)
            except json.JSONDecodeError:
                response = self._error(None, -32700, "Invalid JSON received.")
            else:
                if not isinstance(message, dict):
                    response = self._error(None, -32600, "MCP messages must be JSON objects.")
                else:
                    response = await self.handle_message(message)

            if response is not None:
                stdout.write(json.dumps(response, ensure_ascii=False) + "\n")
                stdout.flush()

    async def iter_responses(self, messages: AsyncIterator[str]) -> AsyncIterator[str]:
        async for raw_message in messages:
            response = await self.handle_message(json.loads(raw_message))
            if response is not None:
                yield json.dumps(response, ensure_ascii=False)
