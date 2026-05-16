# Product Scope

This project is intended to cover the same user-facing surface area as `cardputer-claude-os`, but with OpenAI Codex as the backend agent system.

The goal is not to clone the implementation details one-for-one. The goal is feature parity at the product level, adapted to Codex app-server, Windows middleware, and a flashable Cardputer firmware binary.

## Reference Feature Set

The reference project exposes five visible product areas:

1. A device-host companion mode for live agent status.
2. A voice-first prompt mode with a keyboard fallback.
3. A pager-style interface for long-running remote sessions.
4. A local MCP-style bridge for notifications and approvals.
5. A small launcher bundle that installs as a usable device OS.

## Codex Equivalent Scope

### 1. Codex Buddy

Purpose: pair the Cardputer with a local or remote Codex runtime and surface useful run-state telemetry on the device.

Planned capabilities:

- device connection to the Windows middleware;
- live Codex session state;
- queue depth or turn state where available;
- token or cost telemetry where available;
- compact status banners for activity, idle, waiting, and approval states;
- reconnect and recovery behavior after link loss.

### 2. Push to Codex

Purpose: hold-to-talk or type-to-send input that becomes a Codex turn.

Planned capabilities:

- push-to-talk capture;
- typed prompt mode;
- voice transcription on the Windows side;
- streaming reply rendering on the Cardputer;
- prompt reset and cancel shortcuts;
- per-session context persistence in the middleware, not on the microcontroller.

### 3. Codex Pager

Purpose: manage long-running Codex work from the Cardputer like a pocket pager.

Planned capabilities:

- inbox of active or recent sessions;
- session detail view with live status and deltas;
- interrupt and follow-up reply actions;
- pending tool approval display;
- compact event ticker for background work;
- terminal-style progress summaries.

### 4. Cardputer MCP Bridge

Purpose: expose a minimal local control plane for notifications, questions, and confirmations.

Planned capabilities:

- banner notifications;
- multiple-choice prompts;
- physical confirmation for destructive actions;
- local-only control path when Wi-Fi is unavailable;
- future support for Codex-side integrations that need a small secure human-in-the-loop bridge.

### 5. Launcher Bundle

Purpose: ship a device experience that starts from M5 Launcher and boots into the Cardputer Codex terminal.

Planned capabilities:

- flashable firmware `.bin`;
- launcher entry for the Codex app suite;
- app-level separation for voice, pager, and bridge behaviors;
- clean recovery path back to stock firmware if needed.

## Explicit Non-Goals For The First Pass

- Recreating Claude-specific branding, APIs, or cloud workflows.
- Hard-coding desktop-only assumptions into the firmware.
- Storing conversation state solely on the Cardputer.
- Making the firmware depend on a browser or a full desktop GUI.

## Feature Parity Matrix

| Reference feature | Codex equivalent |
| --- | --- |
| Claude Buddy | Codex Buddy |
| Push to Claude | Push to Codex |
| Claude Pager | Codex Pager |
| Cardputer MCP | Cardputer MCP Bridge |
| Launcher bundle | M5 Launcher-compatible Codex firmware bundle |

## Acceptance Criteria

- The device can send a prompt to Codex from keyboard or voice.
- The device can display streaming Codex responses.
- The device can surface pending approvals.
- The device can manage long-running Codex tasks from a pager-style UI.
- The delivered firmware compiles to a single `.bin` file suitable for M5 Launcher.
