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


@dataclass(slots=True)
class DevPreviewMirror:
    app: Any
    workspace_root: Path
    mirror_dir_name: str = ".cardputer-dev"
    max_events: int = 200
    max_logs: int = 250
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
        return {
            "workspace_root": str(self.workspace_root),
            "mirror_dir": str(self.mirror_dir),
            "last_summary": self.last_summary,
            "session": {
                "workspace_path": session.workspace_path,
                "branch": session.branch,
                "thread_id": session.thread_id,
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
            "recent_events": self.recent_events[-25:],
            "log_lines": self.log_lines[-50:],
            "screen_path": str(self.screen_path),
            "log_path": str(self.log_path),
            "events_path": str(self.events_path),
            "screen_text": self.screen_text,
            "explicit_screen_text": self.explicit_screen_text,
            "firmware_name": self.last_firmware_name,
            "network_status_line": self.last_network_status_line,
            "input_line": self.last_input_line,
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
        return CardputerMessage(message_id, CardputerMessageType(message_type), message_payload)

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
        state = self.snapshot()
        session = state["session"]
        return f"""<!doctype html>
<html lang="fr">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Cardputer Codex Preview</title>
  <style>
    :root {{
      color-scheme: dark;
      --bg: #0b1020;
      --panel: #10192e;
      --panel-2: #0d1629;
      --line: #22304d;
      --text: #e7ecf7;
      --muted: #9db0d1;
      --accent: #84b8ff;
      --accent-2: #63e6be;
      --danger: #ff8080;
      --shadow: 0 10px 36px rgba(0, 0, 0, 0.32);
      --radius: 8px;
      font-family: ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    }}
    body {{
      margin: 0;
      background: linear-gradient(180deg, #0a0f1b 0%, #0b1020 100%);
      color: var(--text);
    }}
    .shell {{
      display: grid;
      grid-template-columns: minmax(320px, 1fr) minmax(300px, 420px);
      gap: 16px;
      padding: 16px;
      min-height: 100vh;
      box-sizing: border-box;
    }}
    .panel {{
      background: rgba(16, 25, 46, 0.95);
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
    .screen-shell {{
      display: grid;
      gap: 12px;
      padding: 14px;
      justify-items: start;
    }}
    .screen-label {{
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 12px;
      font-size: 12px;
      color: var(--muted);
    }}
    .screen-device {{
      position: relative;
      width: min(100%, 240px);
      aspect-ratio: 240 / 135;
      max-height: 135px;
      background: #000;
      border: 1px solid #1f1f1f;
      border-radius: 2px;
      box-shadow: none;
      padding: 0;
      box-sizing: border-box;
      overflow: hidden;
    }}
    .screen-panel {{
      position: relative;
      width: 100%;
      height: 100%;
      background: #000;
      overflow: hidden;
      font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
      color: #ffffff;
      letter-spacing: 0;
      image-rendering: pixelated;
    }}
    .screen-statusbar {{
      position: absolute;
      inset: 0 0 auto 0;
      height: 20px;
      padding: 0 4px;
      background: #404040;
      color: #ffffff;
      border-bottom: 1px solid #303030;
      box-sizing: border-box;
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 8px;
      font-size: 8px;
      line-height: 1;
      overflow: hidden;
    }}
    .screen-meta {{
      position: absolute;
      left: 0;
      top: 20px;
      width: 100%;
      height: 22px;
      padding: 3px 4px 0;
      box-sizing: border-box;
      color: #ffffff;
      font-size: 8px;
      line-height: 1.1;
      overflow: hidden;
      white-space: nowrap;
    }}
    .screen-meta-row {{
      display: flex;
      align-items: center;
      gap: 4px;
      overflow: hidden;
    }}
    .screen-meta-label {{
      min-width: 42px;
    }}
    .screen-content {{
      position: absolute;
      left: 4px;
      top: 42px;
      width: calc(100% - 8px);
      height: calc(100% - 64px);
      margin: 0;
      padding: 3px 4px;
      overflow: hidden;
      background: #000;
      color: #ffffff;
      font-size: 7px;
      line-height: 1.12;
      letter-spacing: 0;
      box-sizing: border-box;
      white-space: pre-wrap;
      word-break: break-word;
      border: 1px solid #404040;
    }}
    .screen-footer {{
      position: absolute;
      left: 0;
      right: 0;
      bottom: 0;
      height: 20px;
      padding: 0 4px;
      background: #404040;
      color: #ffffff;
      font-size: 8px;
      border-top: 1px solid #303030;
      box-sizing: border-box;
      display: flex;
      align-items: center;
      overflow: hidden;
    }}
    .right {{
      display: grid;
      gap: 16px;
      align-content: start;
    }}
    .session-strip {{
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 8px;
      padding: 14px;
      font-size: 12px;
      color: var(--muted);
      border-bottom: 1px solid var(--line);
      background: rgba(13, 22, 41, 0.7);
    }}
    .session-strip strong {{
      color: var(--text);
      font-weight: 600;
    }}
    .controls {{
      display: grid;
      gap: 12px;
      padding: 14px;
      border-bottom: 1px solid var(--line);
    }}
    .row {{
      display: grid;
      grid-template-columns: 1fr auto;
      gap: 8px;
    }}
    input, textarea, button {{
      border-radius: 6px;
      border: 1px solid var(--line);
      background: var(--panel-2);
      color: var(--text);
      font: inherit;
    }}
    input, textarea {{
      padding: 10px 12px;
    }}
    textarea {{
      min-height: 76px;
      resize: vertical;
    }}
    button {{
      padding: 10px 12px;
      cursor: pointer;
      background: #17305f;
    }}
    button:hover {{
      background: #21406f;
    }}
    .chiprow {{
      display: flex;
      flex-wrap: wrap;
      gap: 8px;
    }}
    .chiprow button {{
      padding: 8px 10px;
      font-size: 12px;
    }}
    .logs {{
      margin: 0;
      padding: 14px;
      min-height: 260px;
      white-space: pre-wrap;
      font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
      font-size: 12px;
      line-height: 1.45;
      color: #dbe5fb;
    }}
    .footer {{
      padding: 12px 14px;
      border-top: 1px solid var(--line);
      color: var(--muted);
      font-size: 12px;
      word-break: break-all;
    }}
    .accent {{
      color: var(--accent);
    }}
    .danger {{
      color: var(--danger);
    }}
    @media (max-width: 980px) {{
      .shell {{
        grid-template-columns: 1fr;
      }}
    }}
  </style>
</head>
<body>
  <main class="shell">
    <section class="panel">
      <h2>Cardputer mirror</h2>
      <div class="screen-shell">
        <div class="screen-label">
          <span>Simulated Cardputer screen</span>
          <span id="screen-status">{html.escape(str(state['last_summary']))}</span>
        </div>
        <div class="screen-device">
          <div class="screen-panel">
            <div class="screen-statusbar">
              <span id="screen-left">{html.escape(str(state['firmware_name']))}</span>
              <span id="screen-right">{html.escape(str(state['network_status_line']))}</span>
            </div>
            <div class="screen-meta">
              <div class="screen-meta-row">
                <span class="screen-meta-label">App:</span>
                <span id="screen-app">{html.escape('Codex Buddy')}</span>
              </div>
              <div class="screen-meta-row">
                <span class="screen-meta-label">Battery:</span>
                <span id="screen-battery">{html.escape('0%')}</span>
                <span id="screen-battery-mv">{html.escape('0 mV')}</span>
                <span class="screen-meta-label">Cdx:</span>
                <span id="screen-cdx">{html.escape('idl')}</span>
              </div>
            </div>
            <pre class="screen-content" id="screen">{html.escape(state['screen_text'])}</pre>
            <div class="screen-footer">
              <span id="screen-input">&gt; </span>
            </div>
          </div>
        </div>
      </div>
      <div class="footer">
        Mirror files: <span class="accent">{html.escape(state['screen_path'])}</span> |
        <span class="accent">{html.escape(state['log_path'])}</span>
      </div>
    </section>
    <section class="right">
      <section class="panel">
        <h2>Session</h2>
        <div class="session-strip">
          <div><strong>Workspace</strong><br><span id="workspace">{html.escape(str(session['workspace_path']))}</span></div>
          <div><strong>Branch</strong><br><span id="branch">{html.escape(str(session['branch'] or '-'))}</span></div>
          <div><strong>Thread</strong><br><span id="thread">{html.escape(str(session['thread_id'] or '-'))}</span></div>
          <div><strong>Approval</strong><br><span id="approval">{html.escape(str(session['pending_approval_title'] or '-'))}</span></div>
        </div>
      </section>
      <section class="panel">
        <h2>Controls</h2>
        <div class="controls">
          <textarea id="prompt" placeholder="Type a prompt for Codex...">Hello Codex</textarea>
          <div class="row">
            <button id="send">Send prompt</button>
            <button id="status">Refresh status</button>
          </div>
          <div class="chiprow">
            <button data-kind="project_select" data-value=".">Project</button>
            <button data-kind="branch_select" data-value="feature/cardputer">Branch</button>
            <button data-kind="thread_select" data-value="thr_preview">Thread</button>
            <button data-kind="status_request" data-value="">Status</button>
          </div>
          <input id="custom-type" placeholder="message type, e.g. approval_response" />
          <input id="custom-json" placeholder='extra JSON payload, e.g. {{"approved":true}}' />
          <button id="send-custom">Send custom message</button>
        </div>
      </section>
      <section class="panel">
        <h2>Live logs</h2>
        <pre class="logs" id="logs">{html.escape("\n".join(state["log_lines"]))}</pre>
      </section>
    </section>
  </main>
  <script>
    async function refresh() {{
      const response = await fetch('/api/state', {{ cache: 'no-store' }});
      const state = await response.json();
      const session = state.session;
      document.getElementById('workspace').textContent = session.workspace_path || '-';
      document.getElementById('branch').textContent = session.branch || '-';
      document.getElementById('thread').textContent = session.thread_id || '-';
      document.getElementById('approval').textContent = session.pending_approval_title || '-';
      document.getElementById('screen-status').textContent = state.last_summary || '';
      document.getElementById('screen-left').textContent = state.firmware_name || 'cardputer-codex-terminal';
      document.getElementById('screen-right').textContent = state.network_status_line || 'Wi-Fi offline';
      document.getElementById('screen-app').textContent = state.active_app_label || 'Codex Buddy';
      document.getElementById('screen-battery').textContent = (state.battery_percent ?? 0) + '%';
      document.getElementById('screen-battery-mv').textContent = (state.battery_voltage_mv ?? 0) + ' mV';
      document.getElementById('screen-cdx').textContent = state.codex_state_label || 'idl';
      document.getElementById('screen').textContent = state.screen_text || '';
      document.getElementById('screen-input').textContent = '> ' + (state.input_line || '');
      document.getElementById('logs').textContent = (state.log_lines || []).join('\\n');
    }}

    async function send(payload) {{
      await fetch('/api/input', {{
        method: 'POST',
        headers: {{ 'Content-Type': 'application/json' }},
        body: JSON.stringify(payload),
      }});
      await refresh();
    }}

    document.getElementById('send').addEventListener('click', async () => {{
      await send({{ type: 'text_prompt', payload: {{ text: document.getElementById('prompt').value }} }});
    }});

    document.getElementById('status').addEventListener('click', async () => {{
      await send({{ type: 'status_request', payload: {{}} }});
    }});

    document.querySelectorAll('[data-kind]').forEach((button) => {{
      button.addEventListener('click', async () => {{
        const kind = button.dataset.kind;
        const value = button.dataset.value || '';
        if (kind === 'project_select') {{
          await send({{ type: kind, payload: {{ workspace_path: value }} }});
          return;
        }}
        if (kind === 'branch_select') {{
          await send({{ type: kind, payload: {{ branch: value }} }});
          return;
        }}
        if (kind === 'thread_select') {{
          await send({{ type: kind, payload: {{ thread_id: value }} }});
          return;
        }}
        await send({{ type: kind, payload: {{}} }});
      }});
    }});

    document.getElementById('send-custom').addEventListener('click', async () => {{
      const kind = document.getElementById('custom-type').value.trim();
      const extra = document.getElementById('custom-json').value.trim();
      let payload = {{}};
      if (extra) {{
        payload = JSON.parse(extra);
      }}
      await send({{ type: kind || 'text_prompt', payload }});
    }});

    setInterval(refresh, 750);
  </script>
</body>
</html>"""
