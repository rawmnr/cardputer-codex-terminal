# Wiring

## Firmware Build

- Build from `firmware/` with PlatformIO.
- The target artifact is `firmware/cardputer-codex-terminal.bin`.
- The bin is the primary deliverable for M5 Launcher.

## SD Card Config

Write `/cardputer-codex/config.ini` on the Cardputer SD card.

```ini
wifi_ssid=YourNetworkName
wifi_password=YourWiFiPassword
middleware_host=192.168.1.50
middleware_port=8765
middleware_path=/
middleware_token=
```

## Network Rules

- Use a Windows LAN IP, hostname, or VPN address for `middleware_host`.
- Do not use `127.0.0.1`; it points back to the Cardputer itself.
- Keep `bridge_token` and `middleware_token` aligned when the bridge is reachable off-loopback.

## Runtime Artifacts

- `middleware/.cardputer-dev/state.json`
- `middleware/.cardputer-dev/screen.txt`
- `middleware/.cardputer-dev/log.txt`
- `middleware/.cardputer-dev/events.jsonl`

