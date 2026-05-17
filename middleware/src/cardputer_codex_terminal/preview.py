from __future__ import annotations

import asyncio
import html
import json
import threading
import traceback
from dataclasses import dataclass, field
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any

from .events import Event
from .messages import CardputerMessage, CardputerMessageType


def _short_payload(payload: dict[str, Any]) -> str:
    content = payload.get("content")
    if isinstance(content, str) and content:
        return content
    status_line = payload.get("status_line")
    if isinstance(status_line, str) and status_line:
        return status_line
    kind = payload.get("kind")
    if isinstance(kind, str) and kind:
        return kind
    return json.dumps(payload, ensure_ascii=False)


def _short_text(value: str, limit: int = 84) -> str:
    text = value.strip()
    if len(text) <= limit:
        return text
    if limit <= 3:
        return text[:limit]
    return text[: limit - 3] + "..."


def _friendly_size(size: int) -> str:
    if size < 1024:
        return f"{size} B"
    if size < 1024 * 1024:
        return f"{size / 1024:.1f} KB"
    return f"{size / (1024 * 1024):.1f} MB"


@dataclass(slots=True)
class DevPreviewMirror:
    app: Any
    workspace_root: Path
    mirror_dir_name: str = ".cardputer-dev"
    max_events: int = 200
    max_logs: int = 250
    max_file_entries: int = 120
    max_event_entries: int = 60
    lock: threading.Lock = field(default_factory=threading.Lock, repr=False)
    mirror_dir: Path = field(init=False, repr=False)
    recent_events: list[dict[str, Any]] = field(default_factory=list)
    log_lines: list[str] = field(default_factory=list)
    screen_text: str = ""
    explicit_screen_text: str | None = None
    last_firmware_name: str = "cardputer-codex-terminal"
    last_network_status_line: str = "Wi-Fi offline"
    last_input_line: str = ""
    last_summary: str = "Preview idle"

    def __post_init__(self) -> None:
        self.workspace_root = self.workspace_root.resolve()
        self.mirror_dir = self.workspace_root / self.mirror_dir_name
        self.mirror_dir.mkdir(parents=True, exist_ok=True)
        self.refresh()

    @property
    def state_path(self) -> Path:
        return self.mirror_dir / "state.json"

    @property
    def screen_path(self) -> Path:
        return self.mirror_dir / "screen.txt"

    @property
    def log_path(self) -> Path:
        return self.mirror_dir / "log.txt"

    @property
    def events_path(self) -> Path:
        return self.mirror_dir / "events.jsonl"

    def record_events(self, events: list[Event]) -> None:
        if not events:
            return

        with self.lock:
            for event in events:
                event_dict = event.to_dict()
                self.recent_events.append(event_dict)
                if len(self.recent_events) > self.max_events:
                    self.recent_events = self.recent_events[-self.max_events :]
                self.log_lines.append(self._format_log_line(event_dict))
                if len(self.log_lines) > self.max_logs:
                    self.log_lines = self.log_lines[-self.max_logs :]
                self.last_summary = self._format_summary(event_dict)
                if event_dict.get("type") == "display_snapshot":
                    payload = event_dict.get("payload")
                    if isinstance(payload, dict):
                        screen_text = payload.get("screen_text")
                        if isinstance(screen_text, str) and screen_text:
                            self.explicit_screen_text = screen_text
                        firmware_name = payload.get("firmware_name")
                        if isinstance(firmware_name, str) and firmware_name:
                            self.last_firmware_name = firmware_name
                        network_status_line = payload.get("network_status_line")
                        if isinstance(network_status_line, str) and network_status_line:
                            self.last_network_status_line = network_status_line
                        input_line = payload.get("input_line")
                        if isinstance(input_line, str):
                            self.last_input_line = input_line
                        status_line = payload.get("status_line")
                        if isinstance(status_line, str) and status_line:
                            self.last_summary = status_line

            self.refresh()

    def refresh(self) -> None:
        snapshot = self.snapshot()
        self.screen_text = self.explicit_screen_text or self._render_screen(snapshot)
        snapshot["screen_text"] = self.screen_text
        self._write_file(self.state_path, json.dumps(snapshot, ensure_ascii=False, indent=2))
        self._write_file(self.screen_path, self.screen_text)
        self._write_file(self.log_path, "\n".join(self.log_lines) + ("\n" if self.log_lines else ""))
        self._write_file(self.events_path, "".join(json.dumps(event, ensure_ascii=False) + "\n" for event in self.recent_events))

    def snapshot(self) -> dict[str, Any]:
        session = self.app.session
        session_index = self.app.session_index.to_dict()
        return {
            "workspace_root": str(self.workspace_root),
            "mirror_dir": str(self.mirror_dir),
            "last_summary": self.last_summary,
            "active_session_id": session_index["active_session_id"],
            "session": {
                "workspace_path": session.workspace_path,
                "branch": session.branch,
                "thread_id": session.thread_id,
                "title": session.title,
                "status": session.status,
                "last_event": session.last_event,
                "pending_approval_id": session.pending_approval_id,
                "pending_approval_title": session.pending_approval_title,
                "pending_approval_detail": session.pending_approval_detail,
                "pending_approval_timeout_seconds": session.pending_approval_timeout_seconds,
                "bridge_prompt_kind": session.bridge_prompt_kind,
                "bridge_prompt_title": session.bridge_prompt_title,
                "bridge_prompt_detail": session.bridge_prompt_detail,
                "bridge_prompt_options": list(session.bridge_prompt_options),
                "bridge_prompt_selected_index": session.bridge_prompt_selected_index,
            },
            "session_index": session_index,
            "sessions": list(session_index["sessions"].values()),
            "recent_events": self.recent_events[-self.max_event_entries :],
            "log_lines": self.log_lines[-50:],
            "workspace_files": self._workspace_files(),
            "screen_path": str(self.screen_path),
            "log_path": str(self.log_path),
            "events_path": str(self.events_path),
            "screen_text": self.screen_text,
            "explicit_screen_text": self.explicit_screen_text,
            "firmware_name": self.last_firmware_name,
            "network_status_line": self.last_network_status_line,
            "input_line": self.last_input_line,
        }

    def _workspace_files(self) -> list[dict[str, Any]]:
        files: list[dict[str, Any]] = []
        excluded = {".git", ".pio", "__pycache__", "node_modules", ".venv"}
        try:
            for path in sorted(self.workspace_root.rglob("*")):
                if len(files) >= self.max_file_entries:
                    break
                if not path.is_file():
                    continue
                if any(part in excluded for part in path.parts):
                    continue
                try:
                    relative = path.relative_to(self.workspace_root)
                    stat = path.stat()
                except OSError:
                    continue
                files.append(
                    {
                        "path": relative.as_posix(),
                        "size": stat.st_size,
                        "size_label": _friendly_size(stat.st_size),
                        "mtime": int(stat.st_mtime),
                    }
                )
        except OSError:
            return []
        return files

    def read_workspace_file(self, relative_path: str, max_bytes: int = 12000) -> dict[str, Any]:
        candidate = (self.workspace_root / relative_path).resolve()
        if self.workspace_root not in candidate.parents and candidate != self.workspace_root:
            raise ValueError("File path escapes the workspace root.")
        if not candidate.is_file():
            raise FileNotFoundError(relative_path)
        raw = candidate.read_bytes()
        truncated = len(raw) > max_bytes
        text = raw[:max_bytes].decode("utf-8", errors="replace")
        return {
            "path": relative_path,
            "content": text,
            "truncated": truncated,
            "size": len(raw),
            "size_label": _friendly_size(len(raw)),
        }

    def _format_log_line(self, event: dict[str, Any]) -> str:
        payload = event.get("payload")
        if not isinstance(payload, dict):
            payload = {}
        return f"[{event.get('type', 'event')}] {_short_payload(payload)}"

    def _format_summary(self, event: dict[str, Any]) -> str:
        event_type = event.get("type", "event")
        payload = event.get("payload")
        if not isinstance(payload, dict):
            payload = {}
        return f"{event_type}: {_short_payload(payload)}"

    def _render_screen(self, snapshot: dict[str, Any]) -> str:
        session = snapshot["session"]
        lines = [
            "Cardputer Codex Dev Mirror",
            f"Workspace: {session['workspace_path']}",
            f"Branch: {session['branch'] or '-'}",
            f"Thread: {session['thread_id'] or '-'}",
            f"Approval: {session['pending_approval_title'] or '-'}",
            f"Bridge: {session['bridge_prompt_title'] or '-'}",
            f"Status: {snapshot['last_summary']}",
            "",
            "Recent events:",
        ]

        for event in snapshot["recent_events"][-8:]:
            if not isinstance(event, dict):
                continue
            lines.append(f"- {event.get('type', 'event')}: {_short_payload(event.get('payload') or {})}")

        lines.extend(
            [
                "",
                "Live files:",
                f"- state: {snapshot['screen_path'].replace('screen.txt', 'state.json')}",
                f"- screen: {snapshot['screen_path']}",
                f"- logs: {snapshot['log_path']}",
                f"- events: {snapshot['events_path']}",
            ]
        )
        return "\n".join(lines)

    def _write_file(self, path: Path, content: str) -> None:
        path.write_text(content, encoding="utf-8")


class DevPreviewServer:
    def __init__(self, app: Any, host: str, port: int, workspace_root: str) -> None:
        self.app = app
        self.host = host
        self.port = port
        self.workspace_root = Path(workspace_root)
        self.mirror = DevPreviewMirror(app=app, workspace_root=self.workspace_root)
        self._loop: asyncio.AbstractEventLoop | None = None
        self._server: ThreadingHTTPServer | None = None
        self._thread: threading.Thread | None = None
        self._message_counter = 1
        self.app.event_observer = self.mirror.record_events

    def bind_loop(self, loop: asyncio.AbstractEventLoop) -> None:
        self._loop = loop

    def start(self) -> None:
        handler = self._build_handler()
        self._server = ThreadingHTTPServer((self.host, self.port), handler)
        self._server.preview_server = self  # type: ignore[attr-defined]
        self._thread = threading.Thread(target=self._server.serve_forever, name="cardputer-preview", daemon=True)
        self._thread.start()

    def close(self) -> None:
        if self._server is not None:
            self._server.shutdown()
            self._server.server_close()
        if self._thread is not None and self._thread.is_alive():
            self._thread.join(timeout=1)
        self._server = None
        self._thread = None

    def submit_message(self, payload: dict[str, Any]) -> dict[str, Any]:
        if self._loop is None:
            raise RuntimeError("Preview server is not bound to an event loop.")
        message = self._message_from_payload(payload)
        future = asyncio.run_coroutine_threadsafe(self.app.handle_cardputer_message(message), self._loop)
        events = future.result(timeout=30)
        return {"events": [event.to_dict() for event in events], "state": self.mirror.snapshot()}

    def snapshot(self) -> dict[str, Any]:
        return self.mirror.snapshot()

    def _message_from_payload(self, payload: dict[str, Any]) -> CardputerMessage:
        message_type = str(payload.get("type") or payload.get("message_type") or "text_prompt")
        raw_payload = payload.get("payload")
        if isinstance(raw_payload, dict):
            message_payload = raw_payload
        else:
            message_payload = {key: value for key, value in payload.items() if key not in {"type", "message_type", "payload"}}
        message_id = payload.get("id")
        if not isinstance(message_id, str) or not message_id:
            message_id = f"preview-msg-{self._message_counter:06d}"
            self._message_counter += 1
        return CardputerMessage(CardputerMessageType(message_type), message_payload, id=message_id)

    def _build_handler(self):
        preview_server = self

        class Handler(BaseHTTPRequestHandler):
            def do_GET(self) -> None:  # noqa: N802
                try:
                    if self.path in {"/", "/index.html"}:
                        self._send_html(preview_server.render_index_html())
                        return
                    if self.path == "/api/state":
                        self._send_json(preview_server.snapshot())
                        return
                    if self.path == "/api/screen.txt":
                        self._send_text(preview_server.mirror.screen_text)
                        return
                    if self.path == "/api/logs.txt":
                        self._send_text("\n".join(preview_server.mirror.log_lines) + "\n")
                        return
                    if self.path == "/api/events.json":
                        self._send_json({"events": preview_server.mirror.recent_events})
                        return
                    if self.path.startswith("/api/file"):
                        from urllib.parse import parse_qs, urlparse

                        query = parse_qs(urlparse(self.path).query)
                        relative_path = query.get("path", [""])[0]
                        if not relative_path:
                            self._send_json({"error": "Missing path parameter."}, status=400)
                            return
                        try:
                            payload = preview_server.mirror.read_workspace_file(relative_path)
                        except Exception as exc:
                            self._send_json({"error": str(exc)}, status=400)
                            return
                        self._send_json(payload)
                        return
                    self.send_error(404, "Not found")
                except Exception as exc:  # pragma: no cover - defensive HTTP boundary
                    traceback.print_exc()
                    self._send_text(f"Preview error: {exc}", status=500)

            def do_POST(self) -> None:  # noqa: N802
                try:
                    if self.path != "/api/input":
                        self.send_error(404, "Not found")
                        return
                    length = int(self.headers.get("Content-Length", "0"))
                    raw = self.rfile.read(length).decode("utf-8")
                    payload = json.loads(raw) if raw else {}
                    if not isinstance(payload, dict):
                        raise ValueError("Payload must be a JSON object.")
                    response = preview_server.submit_message(payload)
                except Exception as exc:  # pragma: no cover - defensive HTTP boundary
                    traceback.print_exc()
                    self._send_json({"error": str(exc)}, status=400)
                    return

                self._send_json(response)

            def log_message(self, format: str, *args: Any) -> None:  # noqa: A003
                return None

            def _send_html(self, content: str) -> None:
                body = content.encode("utf-8")
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            def _send_json(self, payload: dict[str, Any], status: int = 200) -> None:
                body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
                self.send_response(status)
                self.send_header("Content-Type", "application/json; charset=utf-8")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            def _send_text(self, content: str, status: int = 200) -> None:
                body = content.encode("utf-8")
                self.send_response(status)
                self.send_header("Content-Type", "text/plain; charset=utf-8")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            def _render_index(self) -> str:
                return preview_server.render_index_html()

        Handler.__name__ = "DevPreviewRequestHandler"
        return Handler

    def render_index_html(self) -> str:
        return """<!doctype html>
<html lang="fr">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Cardputer Codex Preview</title>
  <style>
    :root {{
      color-scheme: dark;
      --bg: #0b1020;
      --panel: rgba(15, 23, 44, 0.96);
      --panel-2: #0d1629;
      --panel-3: #0a1323;
      --line: #233454;
      --text: #ebf2ff;
      --muted: #97aacf;
      --accent: #89b9ff;
      --accent-2: #73e0b4;
      --danger: #ff8f8f;
      --warn: #ffd36b;
      --shadow: 0 10px 36px rgba(0, 0, 0, 0.32);
      --radius: 10px;
      --radius-sm: 8px;
      font-family: ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    }}
    body {{
      margin: 0;
      background: linear-gradient(180deg, #0a0f1b 0%, #0b1020 100%);
      color: var(--text);
    }}
    * {{ box-sizing: border-box; }}
    .panel {{
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: var(--radius);
      box-shadow: var(--shadow);
      overflow: hidden;
    }}
    .panel h2 {{
      margin: 0;
      padding: 12px 14px;
      font-size: 13px;
      letter-spacing: 0;
      border-bottom: 1px solid var(--line);
      color: var(--muted);
      text-transform: uppercase;
    }}
    .app {{
      display: grid;
      grid-template-columns: 260px minmax(0, 1fr) 360px;
      grid-template-rows: minmax(0, 1fr);
      gap: 14px;
      min-height: 100vh;
      padding: 14px;
    }}
    .sidebar,
    .main,
    .inspector {{
      min-height: 0;
      display: grid;
      gap: 14px;
      align-content: start;
    }}
    .main {{
      grid-template-rows: minmax(0, 1fr) auto;
    }}
    .scroll {{
      min-height: 0;
      overflow: auto;
    }}
    .sessions-list,
    .stream-list,
    .file-list {{
      display: grid;
      gap: 10px;
      padding: 14px;
    }}
    .session-card,
    .event-card,
    .file-pill {{
      border: 1px solid var(--line);
      background: linear-gradient(180deg, rgba(13, 22, 41, 0.95), rgba(10, 18, 34, 0.95));
      border-radius: var(--radius-sm);
      padding: 10px 12px;
    }}
    .session-card {{
      cursor: pointer;
    }}
    .session-card.active {{
      border-color: var(--accent);
      box-shadow: 0 0 0 1px rgba(137, 185, 255, 0.18) inset;
    }}
    .session-head,
    .event-head,
    .file-head {{
      display: flex;
      justify-content: space-between;
      gap: 10px;
      align-items: baseline;
      margin-bottom: 6px;
    }}
    .session-title,
    .event-title,
    .file-title {{
      font-size: 12px;
      font-weight: 700;
      color: var(--text);
    }}
    .session-meta,
    .event-meta,
    .file-meta {{
      font-size: 11px;
      color: var(--muted);
    }}
    .session-status {{
      font-size: 11px;
      text-transform: uppercase;
      letter-spacing: 0.04em;
      color: var(--accent-2);
    }}
    .session-status.approval {{
      color: var(--warn);
    }}
    .session-status.error {{
      color: var(--danger);
    }}
    .session-status.done {{
      color: #a8b1c5;
    }}
    .stream-window {{
      display: grid;
      min-height: 0;
      grid-template-rows: auto minmax(0, 1fr);
    }}
    .stream-toolbar {{
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 12px;
      padding: 12px 14px;
      border-bottom: 1px solid var(--line);
      background: rgba(8, 14, 26, 0.45);
    }}
    .stream-toolbar strong {{
      color: var(--text);
    }}
    .approval-bar {{
      display: flex;
      flex-wrap: wrap;
      gap: 8px;
      align-items: center;
    }}
    .button {{
      border: 1px solid var(--line);
      background: #17305f;
      color: var(--text);
      border-radius: 8px;
      padding: 8px 10px;
      cursor: pointer;
      font: inherit;
    }}
    .button:hover {{
      background: #21406f;
    }}
    .button.danger {{
      background: #5a1d24;
      border-color: #7a2f38;
    }}
    .button.danger:hover {{
      background: #742631;
    }}
    .button.ghost {{
      background: transparent;
    }}
    .composer {{
      display: grid;
      gap: 10px;
      padding: 14px;
      border-top: 1px solid var(--line);
      background: rgba(8, 14, 26, 0.5);
    }}
    .composer textarea {{
      width: 100%;
      min-height: 84px;
      resize: vertical;
      border-radius: 10px;
      border: 1px solid var(--line);
      background: var(--panel-2);
      color: var(--text);
      padding: 10px 12px;
      font: inherit;
    }}
    .composer-row {{
      display: flex;
      gap: 8px;
      flex-wrap: wrap;
      align-items: center;
    }}
    .snapshot-panel {{
      padding: 14px;
      display: grid;
      gap: 12px;
    }}
    .screen-frame {{
      border: 1px solid var(--line);
      border-radius: 10px;
      overflow: hidden;
      background: #000;
    }}
    .screen-topbar,
    .screen-bottombar {{
      display: flex;
      justify-content: space-between;
      gap: 8px;
      align-items: center;
      padding: 6px 8px;
      font-size: 11px;
      background: #3c3c3c;
      color: #fff;
    }}
    .screen-body {{
      background: #000;
      color: #fff;
      font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
      font-size: 11px;
      line-height: 1.35;
      white-space: pre-wrap;
      word-break: break-word;
      min-height: 160px;
      padding: 8px;
    }}
    .file-list-wrap {{
      display: grid;
      gap: 10px;
      padding: 14px;
    }}
    .file-pill {{
      cursor: pointer;
      display: grid;
      gap: 4px;
    }}
    .file-pill.active {{
      border-color: var(--accent);
    }}
    .file-preview {{
      border: 1px solid var(--line);
      border-radius: 10px;
      padding: 12px;
      background: #07101e;
      color: #dbe7ff;
      font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
      font-size: 12px;
      white-space: pre-wrap;
      word-break: break-word;
      min-height: 140px;
      max-height: 280px;
      overflow: auto;
    }}
    .hint {{
      color: var(--muted);
      font-size: 12px;
      line-height: 1.4;
    }}
    .tiny {{
      font-size: 11px;
      color: var(--muted);
    }}
    .badge {{
      display: inline-flex;
      align-items: center;
      gap: 6px;
      padding: 2px 8px;
      border-radius: 999px;
      background: rgba(137, 185, 255, 0.12);
      color: var(--accent);
      font-size: 11px;
    }}
    .stream-item {{
      margin-top: 10px;
    }}
    .stream-item:first-child {{
      margin-top: 0;
    }}
    @media (max-width: 1200px) {{
      .app {{
        grid-template-columns: 1fr;
      }}
    }}
    @media (max-width: 700px) {{
      .composer-row {{
        display: grid;
      }}
      .button {{
        width: 100%;
      }}
    }}
    .hidden {{
      display: none !important;
    }}
  </style>
</head>
<body>
  <main class="app">
    <aside class="sidebar panel">
      <h2>Sessions</h2>
      <div class="sessions-list scroll" id="sessions"></div>
    </aside>

    <section class="main">
      <section class="panel stream-window">
        <div class="stream-toolbar">
          <div>
            <strong>Event Stream</strong>
            <div class="tiny" id="stream-subtitle">Loading session data...</div>
          </div>
          <div class="approval-bar" id="approval-bar"></div>
        </div>
        <div class="scroll" id="stream"></div>
      </section>

      <section class="panel composer">
        <div class="composer-row">
          <span class="badge" id="composer-mode">Compose</span>
          <span class="hint" id="composer-hint">Enter sends a new prompt into the active session.</span>
        </div>
        <textarea id="composer-input" placeholder="Type a prompt or reply for the selected Codex session..."></textarea>
        <div class="composer-row">
          <button class="button" id="send-prompt">Send Prompt</button>
          <button class="button ghost" id="send-reply">Reply in Thread</button>
          <button class="button ghost" id="refresh-state">Refresh</button>
        </div>
      </section>
    </section>

    <aside class="inspector">
      <section class="panel">
        <h2>Cardputer Snapshot</h2>
        <div class="snapshot-panel">
          <div class="hint" id="snapshot-meta">Waiting for snapshot...</div>
          <div class="screen-frame">
            <div class="screen-topbar">
              <span id="snapshot-top-left">cardputer-codex-terminal</span>
              <span id="snapshot-top-right">Wi-Fi offline</span>
            </div>
            <div class="screen-body" id="snapshot-screen"></div>
            <div class="screen-bottombar">
              <span id="snapshot-bottombar-left">App: Codex Buddy</span>
              <span id="snapshot-bottombar-right">Ready</span>
            </div>
          </div>
        </div>
      </section>

      <section class="panel">
        <h2>Workspace Files</h2>
        <div class="file-list-wrap">
          <div class="hint">Browse mirrored artifacts and workspace files. Click a pill to inspect its contents.</div>
          <div class="file-list scroll" id="files"></div>
          <div class="file-preview" id="file-preview">Select a file to preview it here.</div>
        </div>
      </section>
    </aside>
  </main>

  <script>
    const stateCache = {{
      selectedSessionId: null,
      selectedFilePath: null,
      previewText: "",
    }};

    function byId(id) {{
      return document.getElementById(id);
    }}

    function sessionList(state) {{
      const index = state.session_index || {{}};
      const sessions = state.sessions || Object.values(index.sessions || {{}}) || [];
      return sessions;
    }}

    function selectedSession(state) {{
      const sessions = sessionList(state);
      if (!sessions.length) {{
        return null;
      }}
      const activeId = stateCache.selectedSessionId || state.active_session_id || (state.session_index && state.session_index.active_session_id) || null;
      if (activeId) {{
        const found = sessions.find((session) => session.session_id === activeId);
        if (found) {{
          return found;
        }}
      }}
      return sessions[0];
    }}

    function sessionStatusClass(status) {{
      const value = String(status || "idle").toLowerCase();
      if (value === "approval") return "approval";
      if (value === "error") return "error";
      if (value === "done") return "done";
      return "";
    }}

    function renderSessions(state) {{
      const container = byId("sessions");
      const sessions = sessionList(state);
      if (!sessions.length) {{
        container.innerHTML = '<div class="hint">No sessions yet. Start a prompt to populate this list.</div>';
        return;
      }}

      const active = selectedSession(state);
      container.innerHTML = sessions.map((session) => {{
        const isActive = active && session.session_id === active.session_id;
        const status = String(session.status || "idle");
        const last = session.last_event || "No recent event";
        const thread = session.thread_id || "No thread yet";
        const pending = session.pending_approval_id ? '<span class="badge">Approval pending</span>' : '';
        return `
          <div class="session-card ${isActive ? "active" : ""}" data-session-id="${session.session_id}">
            <div class="session-head">
              <div>
                <div class="session-title">${escapeHtml(session.title || session.session_id || "Untitled session")}</div>
                <div class="session-meta">${escapeHtml(session.workspace_path || ".")} / ${escapeHtml(session.branch || "-")}</div>
              </div>
              <div class="session-status ${sessionStatusClass(status)}">${escapeHtml(status)}</div>
            </div>
            <div class="session-meta">${escapeHtml(last)}</div>
            <div class="session-meta">${escapeHtml(thread)}</div>
            <div style="margin-top:8px; display:flex; gap:8px; flex-wrap:wrap;">
              ${pending}
            </div>
          </div>
        `;
      }}).join("");

      container.querySelectorAll("[data-session-id]").forEach((node) => {{
        node.addEventListener("click", async () => {{
          const sessionId = node.getAttribute("data-session-id");
          const session = sessions.find((item) => item.session_id === sessionId);
          if (!session) {{
            return;
          }}
          stateCache.selectedSessionId = session.session_id;
          if (session.thread_id) {{
            await send({{ type: "thread_select", payload: {{ thread_id: session.thread_id }} }});
          }}
          renderComposerHint(state, session);
          renderApprovalBar(state, session);
          renderStream(state);
        }});
      }});
    }}

    function renderStream(state) {{
      const container = byId("stream");
      const session = selectedSession(state);
      const events = session && Array.isArray(session.events) && session.events.length ? session.events : (state.recent_events || []);
      if (!events.length) {{
        container.innerHTML = '<div class="stream-list"><div class="hint">No events yet.</div></div>';
        byId("stream-subtitle").textContent = "Waiting for events.";
        return;
      }}

      const subtitle = session
        ? `${session.title || session.session_id || "Session"} · ${session.status || "idle"}`
        : "Recent middleware events";
      byId("stream-subtitle").textContent = subtitle;

      const rendered = events.slice().reverse().map((event) => {{
        const payload = event.payload || {{}};
        const summary = shortPayload(payload);
        return `
          <article class="event-card stream-item">
            <div class="event-head">
              <div class="event-title">${escapeHtml(event.type || "event")}</div>
              <div class="event-meta">${escapeHtml(payload.kind || payload.channel || "")}</div>
            </div>
            <div class="event-meta">${escapeHtml(summary)}</div>
          </article>
        `;
      }}).join("");
      container.innerHTML = `<div class="stream-list">${rendered}</div>`;
    }}

    function renderApprovalBar(state, sessionOverride) {{
      const container = byId("approval-bar");
      const session = sessionOverride || selectedSession(state);
      const approvalId = session && session.pending_approval_id ? session.pending_approval_id : "";
      const detail = session && session.pending_approval_detail ? session.pending_approval_detail : "";
      if (!approvalId) {{
        container.innerHTML = '<span class="tiny">No pending approval</span>';
        return;
      }}

      container.innerHTML = `
        <span class="tiny">${escapeHtml(detail || approvalId)}</span>
        <button class="button" data-approval="yes">Approve</button>
        <button class="button danger" data-approval="no">Deny</button>
      `;
      container.querySelectorAll("[data-approval]").forEach((button) => {{
        button.addEventListener("click", async () => {{
          const approved = button.getAttribute("data-approval") === "yes";
          await send({{
            type: "approval_response",
            payload: {{
              approval_id: approvalId,
              approved,
              note: approved ? "Approved from preview" : "Denied from preview",
            }},
          }});
        }});
      }});
    }}

    function renderComposerHint(state, sessionOverride) {{
      const session = sessionOverride || selectedSession(state);
      const hint = byId("composer-hint");
      const mode = byId("composer-mode");
      if (!session) {{
        mode.textContent = "Compose";
        hint.textContent = "Start a prompt to create a Codex session.";
        return;
      }}
      mode.textContent = session.status === "approval" ? "Approval" : "Compose";
      hint.textContent = `${session.title || session.session_id || "Session"} · ${session.workspace_path || "."} · ${session.branch || "-"}`;
    }}

    function renderSnapshot(state) {{
      const session = selectedSession(state) || (state.session || {{}});
      byId("snapshot-meta").textContent = `${state.last_summary || "Preview idle"} · ${state.active_session_id || "no active session"}`;
      byId("snapshot-top-left").textContent = state.firmware_name || "cardputer-codex-terminal";
      byId("snapshot-top-right").textContent = state.network_status_line || "Wi-Fi offline";
      byId("snapshot-screen").textContent = state.screen_text || "";
      byId("snapshot-bottombar-left").textContent = `App: ${session.title || (state.session && state.session.title) || "Codex Buddy"}`;
      byId("snapshot-bottombar-right").textContent = `${session.status || "idle"} / ${session.last_event || "waiting"}`;
    }}

    function renderFiles(state) {{
      const container = byId("files");
      const files = state.workspace_files || [];
      if (!files.length) {{
        container.innerHTML = '<div class="hint">No workspace files found.</div>';
        return;
      }}

      container.innerHTML = files.map((file) => {{
        const active = stateCache.selectedFilePath === file.path;
        return `
          <div class="file-pill ${active ? "active" : ""}" data-file-path="${encodeURIComponent(file.path)}">
            <div class="file-head">
              <div class="file-title">${escapeHtml(file.path)}</div>
              <div class="file-meta">${escapeHtml(file.size_label || "")}</div>
            </div>
            <div class="file-meta">${escapeHtml(new Date((file.mtime || 0) * 1000).toLocaleString())}</div>
          </div>
        `;
      }}).join("");

      container.querySelectorAll("[data-file-path]").forEach((node) => {{
        node.addEventListener("click", async () => {{
          const path = decodeURIComponent(node.getAttribute("data-file-path") || "");
          if (!path) {{
            return;
          }}
          stateCache.selectedFilePath = path;
          await loadFile(path);
          renderFiles(state);
        }});
      }});
    }}

    async function loadFile(path) {{
      const preview = byId("file-preview");
      preview.textContent = "Loading " + path + "...";
      try {{
        const response = await fetch("/api/file?path=" + encodeURIComponent(path), {{ cache: "no-store" }});
        const data = await response.json();
        if (!response.ok) {{
          preview.textContent = data.error || "Failed to load file.";
          return;
        }}
        stateCache.previewText = data.content || "";
        preview.textContent = `${data.path}\\n${data.size_label || ""}${data.truncated ? " (truncated)" : ""}\\n\\n${data.content || ""}`;
      }} catch (error) {{
        preview.textContent = String(error);
      }}
    }}

    async function refresh() {{
      const response = await fetch("/api/state", {{ cache: "no-store" }});
      const state = await response.json();
      const session = selectedSession(state);
      if (!stateCache.selectedSessionId) {{
        stateCache.selectedSessionId = state.active_session_id || (state.session_index && state.session_index.active_session_id) || null;
      }}
      renderSessions(state);
      renderStream(state);
      renderApprovalBar(state, session);
      renderComposerHint(state, session);
      renderSnapshot(state);
      renderFiles(state);
      if (stateCache.selectedFilePath && !stateCache.previewText) {{
        await loadFile(stateCache.selectedFilePath);
      }}
    }}

    async function send(payload) {{
      await fetch("/api/input", {{
        method: "POST",
        headers: {{ "Content-Type": "application/json" }},
        body: JSON.stringify(payload),
      }});
      await refresh();
    }}

    function shortPayload(payload) {{
      if (payload.content) return String(payload.content);
      if (payload.status_line) return String(payload.status_line);
      if (payload.kind) return String(payload.kind);
      return JSON.stringify(payload);
    }}

    function escapeHtml(value) {{
      return String(value || "")
        .replaceAll("&", "&amp;")
        .replaceAll("<", "&lt;")
        .replaceAll(">", "&gt;")
        .replaceAll('"', "&quot;")
        .replaceAll("'", "&#39;");
    }}

    byId("send-prompt").addEventListener("click", async () => {{
      const text = byId("composer-input").value;
      await send({{ type: "text_prompt", payload: {{ text }} }});
    }});

    byId("send-reply").addEventListener("click", async () => {{
      const text = byId("composer-input").value;
      await send({{ type: "text_prompt", payload: {{ text }} }});
    }});

    byId("refresh-state").addEventListener("click", refresh);

    byId("composer-input").addEventListener("keydown", async (event) => {{
      if (event.key === "Enter" && !event.shiftKey) {{
        event.preventDefault();
        const text = byId("composer-input").value;
        await send({{ type: "text_prompt", payload: {{ text }} }});
      }}
    }});

    setInterval(refresh, 1000);
    refresh();
  </script>
</body>
</html>""".replace("{{", "{").replace("}}", "}")
