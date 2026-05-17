# Middleware

This folder contains the first Windows bridge prototype between the Cardputer and Codex.

The initial scaffold provides:

- a CLI;
- an event model;
- a Codex transport abstraction;
- a mock for early local testing;
- a stdio Codex app-server transport for the stable real-Codex path;
- an explicit local WebSocket Codex app-server transport for debug / advanced use;
- a WebSocket bridge for Cardputer clients.
- a buffered voice prompt pipeline with a transcriber hook.

The Cardputer message contract is versioned. Current protocol version: `1`.
Cardputer requests now carry a stable `id`, and the middleware replies with an `ack` frame before the semantic response so callers can match responses to pending requests.
The bridge server can also require a shared `bridge_token` for remote access.
The middleware now owns the Codex `SessionIndex` and serializes active/recent session state for the Pager browser instead of asking the microcontroller to keep the model itself.
For faster iteration, the middleware can now run a browser preview that mirrors the latest session state into local files under `.cardputer-dev/`.
The preview is a Central Console with session browsing, event stream, prompt composer, approval buttons, live Cardputer snapshot, and workspace file browsing.
It also has an MCP server mode for Codex, exposed over stdio, so Codex can call the Cardputer directly as tools.

## Tooling

This package is managed with `uv`.

Set up the environment:

```bash
uv sync
```

Run the CLI:

```bash
uv run cardputer-codex-middleware --help
```

Run the bridge server:

```bash
uv run cardputer-codex-middleware --serve --bridge-token <shared-secret>
```

Run against real Codex through the stable stdio app-server transport:

```bash
uv run cardputer-codex-middleware --real-codex --prompt "Hello Codex"
uv run cardputer-codex-middleware --serve --real-codex --bridge-token <shared-secret>
```

The middleware sends JSON-RPC-style app-server messages:

```text
initialize request -> initialized notification -> thread/start request -> turn/start request -> streamed server notifications
```

For version drift checks, generate the protocol schemas for the installed Codex build:

```bash
codex app-server generate-ts --out ./schemas
codex app-server generate-json-schema --out ./schemas
```

WebSocket Codex app-server transport is available for local development only:

```bash
codex app-server --listen ws://127.0.0.1:9000
uv run cardputer-codex-middleware --codex-transport websocket --codex-ws-url ws://127.0.0.1:9000
```

Run the local preview:

```bash
uv run cardputer-codex-middleware --preview
```

Run as a Codex MCP server:

```bash
uv run cardputer-codex-middleware --mcp
```

That mode serves the MCP tools over stdio and keeps the Cardputer WebSocket bridge available on the normal host/port.

The Codex MCP config can point at the middleware like this:

```toml
[mcp_servers.cardputer]
command = "uv"
args = ["run", "cardputer-codex-middleware", "--mcp"]
cwd = "C:\\Gitlab\\cardputer-codex-terminal\\middleware"
tool_timeout_sec = 120
```

The exposed tools are:

- `cardputer.notify(title, body, urgency)`
- `cardputer.ask(question, choices, timeout_s)`
- `cardputer.confirm(title, detail, danger, timeout_s)`
- `cardputer.show(text, channel)`
- `cardputer.dictate(prompt, max_seconds)` placeholder for later

Use `cardputer.confirm` for destructive or irreversible actions where software-only confirmation is not enough.

The preview serves a browser UI at `http://127.0.0.1:8787/` by default and mirrors:

- `.cardputer-dev/state.json`
- `.cardputer-dev/screen.txt`
- `.cardputer-dev/log.txt`
- `.cardputer-dev/events.jsonl`

That gives Codex something stable to inspect live while the firmware loop stays on the mock path.
When the real Cardputer firmware is connected, it also publishes `display_snapshot` events so the preview can show the live on-device screen text instead of only the middleware-generated view.

## Tests

Run the middleware tests with:

```bash
uv run python -m unittest discover -s tests -v
```
