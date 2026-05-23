# Remote Networking

## Need

The Cardputer must reach a remote Windows machine running Codex, even behind NAT or a home firewall.

## Options

| Option | Advantage | Limitation |
| --- | --- | --- |
| Port forwarding | Direct | Exposes the host, fragile with CGNAT |
| Reverse tunnel | Fast for prototyping | Depends on a third party, adds latency, free tier limits |
| Overlay VPN | Stable and secure | More complex embedded integration |

## Target Direction

Use a Tailscale-style overlay VPN with ESP32-compatible embedded integration, for example MicroLink.

Until overlay VPN provisioning is complete, the bridge should still be protected with a shared `bridge_token` so the Windows middleware is not left open on the network.

## Topology

```text
Cardputer -> Wi-Fi -> overlay VPN -> Windows tailnet private IP -> Python middleware
```

## Decisions To Validate

- Selected embedded VPN library.
- Authentication key provisioning strategy.
- Secret rotation and revocation.
- Degraded mode without VPN for local development.

## Current Bridge Provisioning

- Configure the middleware with a shared `bridge_token`.
- Embed the same token in the Cardputer firmware build.
- Rotate by changing both sides together.
- Revoke by clearing the token on the middleware and reflashing the firmware without it or with a new secret.
