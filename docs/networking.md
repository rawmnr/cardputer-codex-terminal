# Networking & Security

The Cardputer connects to the Supervisor (Middleware) via Wi-Fi or BLE. For remote access, an overlay VPN is recommended.


## Options

| Option | Advantage | Complexity |
| :--- | :--- | :--- |
| **Port Forwarding** | Direct, low latency | High (security risk, NAT issues) |
| **Reverse Tunnel** | Easy setup | Medium (latency, 3rd party dependency) |
| **Overlay VPN** | Secure, stable | High (requires ESP32 client support) |


## Target Direction

Use a Tailscale-style overlay VPN with ESP32-compatible embedded integration (e.g., MicroLink).

Until automated, protect the Supervisor bridge with a shared secret:
1. Set `bridge_token` in `/cardputer-codex/config.ini` on the SD card.
2. Match the token in the Middleware CLI: `--bridge-token <secret>`.


## Topology

```text
Cardputer --(Encrypted)--> Wi-Fi/VPN --(bridge_token)--> Supervisor (Middleware)
```



- **Provisioning**: Middleware bridge requires a shared `bridge_token` when exposed beyond loopback.
- **Rotation**: Update `config.ini` and restart the Supervisor process.
- **Revocation**: Clear the token from the Supervisor and update device config.

