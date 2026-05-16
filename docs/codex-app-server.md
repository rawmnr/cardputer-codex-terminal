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
4. Create or resume a thread.
5. Send prompts via `turn/start`.
6. Listen for notifications and deltas.
7. Relay approvals back to the Cardputer.

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
