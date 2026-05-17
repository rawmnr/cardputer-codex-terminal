---
name: cardputer-codex-onboard
description: Codex onboarding for the Cardputer stack. Use when building the firmware with PlatformIO, locating the flashable .bin, preparing /cardputer-codex/config.ini, starting the middleware with uv, starting a Codex app-server, running smoke tests, or diagnosing Wi-Fi, bridge, and token issues.
---

# Cardputer Codex Onboard

## Use This Skill

Use the bundled scripts and references to bring up the Cardputer/Codex stack end to end.

Do not use the Claude OS `install_apps.py` or any UIFlow/MicroPython installer flow. This repository is Codex-native and uses PlatformIO firmware plus Python middleware.

## Workflow

1. Build the firmware from `firmware/` with PlatformIO.
2. Locate `firmware/cardputer-codex-terminal.bin`.
3. Copy the bin to the SD card or the target flash location if needed.
4. Prepare `/cardputer-codex/config.ini` on the Cardputer SD card.
5. Start the middleware with `uv`.
6. Start `codex app-server` only when the middleware needs a local app-server backend.
7. Run smoke tests after wiring changes.

## Scripts

Use the helper scripts in `scripts/` when you want deterministic, repeatable steps:

- `scripts/build_firmware.ps1` - run the PlatformIO build and return the `.bin` path.
- `scripts/copy_bin.ps1` - find `cardputer-codex-terminal.bin` and copy it to a destination.
- `scripts/run_middleware.ps1` - launch the middleware with `uv`.

## References

- `references/wiring.md` - firmware build output, SD config, host naming, and bridge wiring.
- `references/troubleshooting.md` - Wi-Fi, bridge, token, app-server, and smoke-test failures.

## Operating Notes

- Keep complex session state in middleware, not on the Cardputer.
- Treat `middleware_host` as a real Windows/LAN/VPN address, not `127.0.0.1`.
- Keep `bridge_token` and `middleware_token` aligned when the bridge is reachable off-loopback.
- Use physical approval paths for destructive actions.
- Prefer `uv run python -m unittest discover -s tests -v` in `middleware/` and `python -m platformio run` in `firmware/`.

