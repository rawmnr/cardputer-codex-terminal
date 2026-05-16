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
   - Routing of Codex events back to the Cardputer display.

3. **Codex app-server**
   - Thread management.
   - Turn execution.
   - Streaming responses.
   - Approval handling.
   - Access to local tools and MCP.

## Main Flows

### Keyboard Prompt

```text
Cardputer keyboard -> middleware -> turn/start -> Codex -> streaming deltas -> Cardputer display
```

### Voice Prompt

```text
Cardputer microphone -> PCM chunks -> middleware STT -> turn/start -> Codex -> streaming deltas -> Cardputer display
```

### Approval

```text
Codex approval request -> middleware -> Cardputer alert -> user keypress -> middleware -> Codex approval response
```

## Principles

- Keep conversational state on the Codex side, not on the microcontroller.
- Keep the Cardputer as a lightweight, robust, responsive terminal.
- Avoid direct public exposure of the Windows server.
- Separate transport, transcription, and Codex protocol handling cleanly.
- Treat the firmware as a binary deliverable first; source layout should always compile to a M5 Launcher-compatible `.bin`.
