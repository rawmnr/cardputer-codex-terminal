# AGENTS.md

Guidance for Codex agents working in this repository.

## Project Overview

`cardputer-codex-terminal` turns the M5Stack Cardputer ADV into a physical terminal for OpenAI Codex running on a Windows host.

The project is inspired by `dakshaymehta/cardputer-claude-os`, but this repository is a Codex-specific adaptation. Use the Claude OS project at `C:\Users\rom1m\Documents\cardputer-claude-os` as product and interaction reference only; do not copy implementation details blindly.

## Repository Layout

- `firmware/`: Arduino/PlatformIO firmware for the Cardputer.
- `middleware/`: Python Windows bridge managed with `uv`.
- `docs/`: architecture, firmware, middleware, networking, security, and roadmap notes.
- `hardware/`: hardware notes and board references.

## Core Architecture

```text
M5Stack Cardputer ADV
  -> Wi-Fi / overlay VPN
  -> Windows middleware WebSocket bridge
  -> Codex app-server JSON-RPC transport
  -> local Windows workspace and tools
```

Keep the Cardputer lightweight. Conversation state, Codex protocol details, audio buffering/transcription, approval handling, and workspace context belong in the middleware whenever possible.

## Middleware Development

Work from `middleware/`.

Common commands:

```bash
uv sync
uv run cardputer-codex-middleware --help
uv run cardputer-codex-middleware --serve
uv run cardputer-codex-middleware --serve --bridge-token <shared-secret>
uv run cardputer-codex-middleware --real-codex --prompt "Hello Codex"
uv run cardputer-codex-middleware --preview
uv run pytest tests -v
```

Important details:

- Package source lives in `middleware/src/cardputer_codex_terminal/`.
- Tests live in `middleware/tests/` and use `pytest`.
- Cardputer-facing messages use the versioned envelope in `messages.py`.
- Current protocol version is `2`.
- Preserve the message contract unless you also update tests, firmware serialization, and docs.
- Real Codex integration should use `StdioCodexAppServerTransport` by default.
- `LocalWebSocketCodexTransport` is for loopback debug / advanced use only.
- The official app-server flow is `initialize`, `initialized`, `thread/start`, `turn/start`, then streamed server notifications.
- Regenerate app-server schemas after Codex upgrades with `codex app-server generate-ts --out ./schemas` and `codex app-server generate-json-schema --out ./schemas`.
- Prefer async, non-blocking middleware code.
- Avoid logging secrets, bridge tokens, sensitive prompts, or raw audio content.
- The local preview mirrors files under `middleware/.cardputer-dev/`; treat those as generated development artifacts.

## Firmware Development

Work from `firmware/`.

Common command:

```bash
python -m platformio run
```

Expected build output:

```text
firmware/cardputer-codex-terminal.bin
```

Important details:

- Firmware source lives in `firmware/src/`.
- PlatformIO config is `firmware/platformio.ini`.
- The firmware target is a single M5 Launcher-compatible `.bin`.
- Use Arduino/C++17 style consistent with the existing code.
- Keep heap use and display rendering conservative for the 240 x 135 screen.
- Do not put complex Codex session logic in firmware.
- Wi-Fi and middleware settings are read from `/cardputer-codex/config.ini` on the SD card.
- Firmware may append diagnostics to `/cardputer-codex/log.txt`; avoid writing secrets.

## Message and UX Constraints

- Maintain compatibility between firmware message JSON and middleware `CardputerMessage`.
- Use `bridge_token` authentication when the bridge is reachable beyond loopback.
- Approval prompts must support accept/reject paths from physical keys.
- Push-to-talk should stream audio chunks, then send a ready event when the key is released.
- Display snapshots from firmware help the middleware preview track the real on-device screen.
- Keep UI text short enough for the Cardputer display and avoid verbose desktop-style output.

## Documentation

Update docs when behavior or contracts change:

- `docs/architecture.md` for layer and flow changes.
- `docs/middleware.md` for bridge, CLI, message, or preview changes.
- `docs/firmware.md` for firmware behavior, SD config, keys, or build changes.
- `docs/security.md` for auth, token, networking, or exposure changes.

## Testing Expectations

Before finishing middleware work, run:

```bash
cd middleware
uv sync --dev
uv run pytest tests -v
```

Before finishing firmware work, run:

```bash
cd firmware
python -m platformio run
```

If a tool is unavailable locally, report that clearly and include what was not verified.

## Working Rules

- Use `rg` for searching.
- Keep changes scoped to the requested feature or bug.
- Do not revert unrelated user changes in the working tree.
- Prefer existing abstractions over new framework-level changes.
- When delegating investigations, surgical edits, or reviews, prefer the cavecrew investigator/builder/reviewer subagents from `skill://cavecrew`.
- Update tests with behavior changes.
- Treat generated logs, preview files, build outputs, and local config as non-source artifacts unless explicitly asked otherwise.
## Scratch Log

For refactors that touch multiple files or require design tradeoffs, maintain a short scratch log in `LOG.md` at the repo root while working.

Before starting, read `LOG.md` for prior decisions.

Log only decision-grade notes:
- assumptions that mattered
- tradeoffs considered and chosen
- review findings and fixes
- things that were ambiguous or underspecified
- follow-up risks you want the next reader to see

Update the log after each meaningful decision, and update it again before commits.

Keep it terse and current. Delete stale decisions and replace them with the new one when the old note is no longer true. Remove or archive it when the work is done if it is no longer useful.
