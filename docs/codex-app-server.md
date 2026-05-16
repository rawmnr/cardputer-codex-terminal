# Codex App Server Integration

## Transport

The middleware targets a local WebSocket connection to:

```text
codex app-server --listen ws://127.0.0.1:PORT
```

If the server is exposed beyond loopback, token authentication and network restriction become mandatory.

## Session Flow

1. Open the WebSocket.
2. Send `initialize` with client capabilities.
3. Send `initialized`.
4. Create or resume a thread for the selected workspace.
5. Attach git metadata such as the active branch to that thread.
6. Send prompts via `turn/start`.
7. Listen for notifications and deltas.
8. Relay approvals back to the Cardputer.

## Project, Branch, Thread

The Cardputer UI should not model Codex as a single monolithic chat. The useful mental model is:

- **project** = the selected workspace path on the Windows host;
- **branch** = git metadata stored on the active Codex thread;
- **thread** = the Codex conversation/session that owns state.

In practice, the middleware keeps the current workspace and branch in its own session state, then uses `thread/start`, `thread/resume`, and `thread/metadata/update` to keep Codex aligned with what the user selected on the Cardputer.

## Events To Relay

- Item start and completion.
- Agent message deltas.
- Turn status.
- Approval requests.
- Transport or protocol errors.

## Approval Policy

The Cardputer should display a concise request and allow:

- explicit approval;
- explicit rejection;
- configurable timeout;
- minimal tracing on the middleware side.
