# Remote Networking

## Need

The Cardputer must reach a remote Windows machine running Codex, even behind NAT or a home firewall.

## Options

| Option | Advantage | Limitation |
| --- | --- | --- |
| Port forwarding | Simple in theory | Exposes the host, fragile with CGNAT |
| Reverse tunnel | Fast for prototyping | Depends on a third party, adds latency, free tier limits |
| Overlay VPN | Stable and secure | More complex embedded integration |

## Target Direction

Use a Tailscale-style overlay VPN with ESP32-compatible embedded integration, for example MicroLink.

## Topology

```text
Cardputer -> Wi-Fi -> overlay VPN -> Windows tailnet private IP -> Python middleware
```

## Decisions To Validate

- Selected embedded VPN library.
- Authentication key provisioning strategy.
- Secret rotation and revocation.
- Degraded mode without VPN for local development.
