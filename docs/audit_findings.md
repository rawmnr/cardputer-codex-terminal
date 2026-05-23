# Codex Terminal Audit Findings

## Observation Holes
- **Raw JSON streaming**: Terminal output is interleaved with raw JSON messages (artifact://1, artifact://3). Middleware should ideally filter these for clean terminal UX or the firmware should handle them.
- **Port Conflict**: Middleware fails ungracefully if port 8765 is taken (Errno 10048).
- **Mock Codex behavior**: Default one-shot prompt uses mock Codex even if real work is intended (confusing UX).
- **Audio/Binary noise**: If binary data (audio) is sent over the same bridge without being handled by a specific app, it might pollute the console or crash the parser.

## Security Holes
- **Bridge Token Logic**: While `bridge_token` is checked in `server.py`, the initial `HELLO_ACK` (line 125) and `bridge_connected` message (line 40) are sent **before** or **without** token validation.
  - Leak: `workspace_path`, `branch`, `thread_id` are broadcast to any client connecting to the WebSocket before they provide a token.
- **Protocol Downgrade**: `from_dict` in `messages.py` accepts `protocol_version` 1 (default) which might bypass newer security checks if not strictly enforced.
- **RBAC Placeholder**: RBAC is currently a hardcoded "simulated_device_policy" (server.py:152).
- **Token exposure**: `bridge_token` is passed via CLI, potentially visible in process lists.

## Proposed Fixes
- [ ] enforce token check before sending `bridge_connected` or `HELLO_ACK`.
- [ ] add `--secure` flag to hide token from CLI (use env var).
- [ ] implement basic device-id pairing.
