# Architecture

## Goal

Build a mobile physical interface for OpenAI Codex, based on the M5Stack Cardputer ADV, capable of driving a remote Codex session running on a Windows machine.

## Layers

1. **Embedded firmware**
   - Keyboard capture.
   - Push-to-talk audio capture.
   - Agentic stream rendering.
   - WebSocket transport to the middleware.
   - Secure network connectivity via overlay VPN.
   - Build output as a flashable `.bin` for M5 Launcher.

2. **Windows middleware**
   - WebSocket server for the Cardputer.
   - PCM audio reception.
   - Speech-to-text transcription.
   - JSON-RPC client for `codex app-server`.
   - Small orchestration router for `/worktree` and `/run` commands.
   - Codex process supervisor that can isolate YOLO worktree runs in dedicated processes.
   - stdio MCP server for Codex human-in-the-loop tools.
   - Python-owned session and run indexes plus recent-event history for the Pager browser.
   - Routing of Codex events back to the Cardputer display.
   - Local browser preview and mirrored dev files for fast iteration.

3. **Codex app-server**
   - Thread management.
   - Turn execution.
   - Streaming responses.
   - Approval handling.
   - Access to local tools and MCP.

## Main Flows

### Keyboard Prompt

```text
Cardputer keyboard -> middleware bridge -> turn/start -> Codex -> streaming deltas -> Cardputer display
```

### Voice Prompt

```text
Cardputer microphone -> PCM chunks -> middleware STT -> turn/start -> Codex -> streaming deltas -> Cardputer display
```

### Approval

```text
Codex approval request -> middleware -> Cardputer alert -> user keypress -> middleware -> Codex approval response
```

### Pager Browser

```text
Codex session and run events -> middleware SessionIndex + RunIndex -> Cardputer pager inbox/detail -> reply, approval, or session browse actions
```
### Run Orchestration

```text
Cardputer run action -> middleware orchestration router -> worktree/diff/test helpers + Codex process supervisor -> run detail snapshot
```

### Local Bridge Prompt

```text
Codex MCP tool call -> Windows middleware -> Cardputer prompt or notification -> physical selection or confirmation -> middleware tool result
```

## Principles

- Keep conversational state on the Codex side, not on the microcontroller.
- Keep session indexing and history in middleware, and only send serialized snapshots to the Cardputer.
- Keep the Cardputer as a lightweight, robust, responsive terminal.
- Avoid direct public exposure of the Windows server.
- Separate transport, transcription, and Codex protocol handling cleanly.
- Treat the firmware as a binary deliverable first; source layout should always compile to a M5 Launcher-compatible `.bin`.
- For development, prefer the browser preview and mirrored files under `.cardputer-dev/` before flashing the firmware.
- The firmware should publish `display_snapshot` events so the preview can track the real on-device screen text when hardware is connected.
- The firmware now runs a small runtime coordinator with separate UI, network/background, and keyboard tasks; the UI task owns visible state, retained-mode LVGL rendering, and diagnostics, while background work publishes through bounded events instead of mutating the screen directly.
- Keyboard input can wake the keyboard task through an IRQ when hardware provides one, but the same bounded event path remains the fallback when no verified interrupt pin is available.

## Glossary

| Term | Definition |
| :--- | :--- |
| **Codex** | The AI agent (usually `codex-app-server`) running on your host. |
| **Middleware** | The Python supervisor (`cardputer-codex-middleware`) that routes device traffic. |
| **Firmware** | The C++ code running on the Cardputer. |
| **App-Server** | The agentic process managing thread state and tool execution. |
| **MCP** | Model Context Protocol — the standard for AI agents to use local tools. |
| **Pager** | A UI mode for browsing long-running task results and history. |
| **Epoch Sync** | The state-synchronization model used to update the device UI. |
