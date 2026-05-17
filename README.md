# cardputer-codex-terminal

`cardputer-codex-terminal` turns the M5Stack Cardputer ADV into a physical terminal for OpenAI Codex running on a Windows host.

The project is split into two working parts:

- a Cardputer firmware that builds into a flashable `.bin` for M5 Launcher;
- a Windows middleware service that bridges Cardputer input to Codex app-server and back.

The design is inspired by the excellent [`dakshaymehta/cardputer-claude-os`](https://github.com/dakshaymehta/cardputer-claude-os), adapted here for OpenAI Codex.

## What it does

- send typed prompts from the Cardputer keyboard;
- capture push-to-talk audio and forward it to the Windows middleware;
- display Codex status, streaming output, usage, and approvals;
- surface a local MCP-style bridge for notifications, questions, and confirmations;
- keep session context on the middleware side instead of the microcontroller;
- produce a single firmware `.bin` that can be launched from M5 Launcher.

## Current status

The repository already contains working scaffolding and early implementations for:

- a Cardputer firmware shell with app switching;
- `Codex Buddy`, `Push to Codex`, `Codex Pager`, and `Cardputer MCP Bridge` views;
- push-to-talk microphone capture;
- a WebSocket bridge from the firmware to the middleware;
- a Python middleware package managed with `uv`;
- a versioned Cardputer message contract;
- token-based bridge authentication;
- Codex usage and approval tracking;
- a firmware build that emits `firmware/cardputer-codex-terminal.bin`.

## Architecture

```text
M5Stack Cardputer ADV
  | Wi-Fi or overlay VPN
  v
Windows middleware
  | WebSocket / JSON-RPC
  v
Codex app-server
  | local tools / MCP / approvals
  v
Windows workspace
```

## Repository layout

```text
docs/          Architecture, roadmap, hardware, networking, security
firmware/      Cardputer firmware source tree and PlatformIO build
middleware/    Windows Python bridge managed with uv
hardware/      Hardware notes and board-level references
```

## Build and run

### Middleware

```bash
cd middleware
uv sync
uv run cardputer-codex-middleware --help
uv run cardputer-codex-middleware --serve
uv run python -m unittest discover -s tests -v
```

### Firmware

```bash
cd firmware
python -m platformio run
```

The build produces a flashable binary named:

```text
firmware/cardputer-codex-terminal.bin
```

## Documentation

- [Architecture](docs/architecture.md)
- [Product scope](docs/product-scope.md)
- [Firmware](docs/firmware.md)
- [Middleware](docs/middleware.md)
- [Networking](docs/networking.md)
- [Security](docs/security.md)
- [Roadmap](docs/roadmap.md)
- [References](docs/references.md)

## Notes

- The middleware message contract is versioned.
- Remote bridge access can require a shared `bridge_token`.
- The project aims for feature parity with `cardputer-claude-os` at the product level, not a line-by-line port.
