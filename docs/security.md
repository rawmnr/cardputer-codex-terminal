# Security

## Sensitive Surfaces

- Overlay VPN authentication key.
- Possible Codex app-server token.
- User prompts.
- Codex output.
- Shell commands proposed by Codex.
- Access to Windows workspace files.

## Principles

- Do not expose Codex directly to the Internet.
- Prefer loopback between the middleware and Codex.
- Treat Codex app-server WebSocket transport as local-development-only unless it has an explicit authenticated boundary in front of it.
- Keep `YOLO_WORKTREE` runs isolated in a dedicated process rooted at the worktree; do not reuse the shared backend for those runs.
- Reject YOLO on protected branches in middleware before starting a worker.
- Do not run the Cardputer bridge on a non-loopback interface without `bridge_token`.
- Treat the Codex-facing MCP server mode as a local trusted-process surface; launch it only from trusted projects or a shell you control.
- Use the overlay VPN for remote access.
- Require physical confirmation for critical actions.
- Minimize logging by default.
- Plan a clear key revocation procedure.

## Approvals

Approvals are a central part of the design. A destructive or intrusive action should be visible on the Cardputer display before it is accepted.

## Open Questions

- How much detail can be shown on the small screen for long commands.
- Timeout policy.
- Lock mode in case the Cardputer is lost or stolen.
- Local secret encryption on the ESP32-S3.
