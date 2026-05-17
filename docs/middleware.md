# Windows Middleware

## Role

The middleware acts as the bridge between the Cardputer and Codex. It compensates for the microcontroller's limits and isolates the Codex protocol from the firmware.

The Python package in `middleware/` is managed with `uv`:

```bash
uv sync
uv run cardputer-codex-middleware
uv run cardputer-codex-middleware --real-codex --prompt "Hello Codex"
uv run cardputer-codex-middleware --mcp
uv run python -m unittest discover -s tests -v
```

Cardputer-facing messages use a versioned envelope. The current protocol version is `1`.
Each request includes a stable `id`, and the middleware replies with an `ack` frame so the sender can correlate replies with pending requests.
When the bridge is exposed beyond loopback, the server can require a shared `bridge_token` and the firmware must embed the same token in its outgoing envelopes.
The middleware also owns the Codex session index and returns it in `status_request` snapshots so the Pager can browse active and recent sessions without storing the model on-device.
The browser preview surfaces that same session index as a Central Console with session list, event stream, composer, approvals, live screen snapshot, and workspace file browser.

The middleware also exposes a Codex-facing MCP server mode over stdio. In that mode, Codex can launch the middleware directly and invoke physical-human tools instead of going through the app-server bridge first.

## Responsibilities

- Expose a WebSocket server to the Cardputer.
- Receive text commands.
- Receive PCM audio fragments.
- Transcribe audio to text.
- Open a JSON-RPC session to `codex app-server`.
- Start or resume Codex threads.
- Relay deltas and status updates back to the Cardputer.
- Handle approval requests.
- Maintain a `SessionIndex` in Python with `SessionState` records for active and recent Codex threads.
- Return `session_index`, `active_session_id`, and per-session event history in `session_status` payloads.
- Handle local bridge notifications, questions, confirmations, and responses.
- Expose MCP tools for notifications, questions, confirmations, and display-only text.

The middleware CLI supports a `--serve` mode that listens for versioned Cardputer messages over WebSocket and turns them into middleware events.
It refuses non-loopback `--serve` listeners unless `--bridge-token` is provided.

The MCP mode is separate:

```bash
uv run cardputer-codex-middleware --mcp
```

That mode serves the MCP tools over stdio and keeps the Cardputer bridge listener available on the normal host/port.

Codex can point its MCP config at that command:

```toml
[mcp_servers.cardputer]
command = "uv"
args = ["run", "cardputer-codex-middleware", "--mcp"]
cwd = "C:\\Gitlab\\cardputer-codex-terminal\\middleware"
tool_timeout_sec = 120
```

The tool surface is intentionally small:

- `cardputer.notify(title, body, urgency)`
- `cardputer.ask(question, choices, timeout_s)`
- `cardputer.confirm(title, detail, danger, timeout_s)`
- `cardputer.show(text, channel)`
- `cardputer.dictate(prompt, max_seconds)` reserved for later

Use `cardputer.confirm` for destructive or irreversible actions where software-only confirmation is not enough.

## Codex App-Server Transport

The real Codex path uses the official app-server message flow:

```text
initialize -> initialized -> thread/start -> turn/start -> streamed server notifications
```

The transport layer is split into:

- `MockCodexTransport` for local scaffolding and tests.
- `StdioCodexAppServerTransport` for the stable `codex app-server` path.
- `LocalWebSocketCodexTransport` for loopback-only debug / advanced use.

Use stdio for normal real-Codex runs:

```bash
uv run cardputer-codex-middleware --real-codex --prompt "Hello Codex"
uv run cardputer-codex-middleware --serve --real-codex --bridge-token <shared-secret>
```

Use WebSocket only when you explicitly start a local app-server listener:

```bash
codex app-server --listen ws://127.0.0.1:9000
uv run cardputer-codex-middleware --codex-transport websocket --codex-ws-url ws://127.0.0.1:9000
```

The app-server schema should be regenerated when upgrading Codex:

```bash
codex app-server generate-ts --out ./schemas
codex app-server generate-json-schema --out ./schemas
```

The voice pipeline is intentionally split into three steps:

1. collect PCM chunks from the Cardputer;
2. buffer them until the user releases push-to-talk;
3. transcribe the buffered audio into a prompt before handing it to Codex.

## Target Voice Pipeline

```text
PCM int16 -> memory buffer -> optional VAD -> resample to 16 kHz if needed -> faster-whisper -> text -> turn/start
```

## Constraints

- Non-blocking async loop.
- No persistent audio storage by default.
- Careful logging to avoid writing secrets or sensitive prompts.
- Simulation mode is useful before the real firmware exists.
