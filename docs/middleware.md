# Middleware CLI Reference

The `cardputer-codex-middleware` is the heart of the system. It bridges your physical Cardputer to your agentic supervisor.

The Python package in `middleware/` is managed with `uv`:

```bash
uv sync
uv run cardputer-codex-middleware
uv run cardputer-codex-middleware --real-codex --prompt "Hello Codex"
uv run cardputer-codex-middleware --mcp
uv run pytest tests -v
```

Cardputer-facing messages use a versioned envelope. The current protocol version is `2`.
Each request includes a stable `id`, and the middleware replies with an `ack` frame so the sender can correlate replies with pending requests.
When the bridge is exposed beyond loopback, the server can require a shared `bridge_token` and the firmware must embed the same token in its outgoing envelopes.
The middleware also owns the Codex session index and a parallel `RunIndex`; `status_request` snapshots now return both `session_index` and a `runs` list so the Pager can browse active and recent sessions without storing the model on-device.
The browser preview surfaces the same session index and run list as a Central Console with session list, event stream, composer, approvals, live screen snapshot, and workspace file browser.
It now also projects `run_list`, `run_detail`, and `approval_inbox` snapshots so the Cardputer can browse multiple runs, inspect compact diff/test summaries, and route approvals back to the correct run.

The middleware also owns a small orchestration router and a Codex process supervisor. Worker runs in `YOLO_WORKTREE` mode get a dedicated `codex app-server` process rooted at the worktree; safe and review runs can keep using the shared transport.

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
- Maintain a `RunIndex` in Python with `AgentRun` records for active and recent execution runs.
- Route `/worktree`, `/run approve|reject|stop|merge|pause|resume|diff|tests|report`, and `/mode` commands.
- Track `DiffSummary` and `TestSummary` on each run when the backend provides them.
- Return `session_index`, `active_session_id`, `active_run_id`, and per-session/run history in `session_status` payloads.
- Expose MCP tools for notifications, questions, confirmations, and display-only text.


## CLI Usage

### Core Options

| Flag | Description |
| :--- | :--- |
| `--serve` | Start the WebSocket/BLE bridge listener. |
| `--real-codex` | Connect to your actual `codex app-server` via `stdio`. |
| `--mcp` | Run as an MCP server for human-in-the-loop desktop tools. |
| `--preview` | Start the local browser Central Console at `127.0.0.1:8787`. |
| `--bridge-transport` | Select `wifi`, `ble`, or `hybrid` (default `wifi`). |

---

## Connectivity & Transports

### Wi-Fi (WebSocket)
Default transport. Low latency, high bandwidth for voice and snapshots.

### Bluetooth Low Energy (BLE)
For local, low-power connectivity.
1. Run: `uv run cardputer-codex-middleware --serve --bridge-transport ble`
2. The middleware scans for `CardputerCodex_` devices.
3. **Note:** Requires `bleak` installed (`uv sync`).

### Hybrid Mode
Simultaneous Wi-Fi and BLE listeners:
```bash
uv run cardputer-codex-middleware --serve --bridge-transport hybrid
```


---

## Codex Integration

### Supervisor Mode
When running with `--real-codex`, the middleware manages the `codex` process lifecycle. It routes prompts to the agent and relays streaming deltas back to the hardware.

### MCP Mode
Exposes the Cardputer as a set of physical tools for your desktop agent:
- `cardputer.notify(title, body)`
- `cardputer.ask(question, options)`
- `cardputer.confirm(title)`


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
CARDPUTER_BRIDGE_TOKEN=<shared-secret> uv run cardputer-codex-middleware --serve --real-codex
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
