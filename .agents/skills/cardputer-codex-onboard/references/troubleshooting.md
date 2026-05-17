# Troubleshooting

## Wi-Fi

- Check the SD config file for the correct SSID and password.
- Confirm the host address is reachable from the Cardputer network.
- Remember that `127.0.0.1` is wrong for the Windows host.

## Bridge

- If the Cardputer connects but the bridge does not, verify `middleware_host`, `middleware_port`, and `middleware_path`.
- If the bridge is exposed beyond loopback, confirm the same `bridge_token` is set on both sides.
- Check `middleware/.cardputer-dev/log.txt` and the preview event stream for the latest failure.

## Token

- If requests fail with auth errors, compare the SD `middleware_token` with the middleware `bridge_token`.
- Rotate both tokens together.
- Reflash or re-copy the SD config after changing the token.

## App-Server

- Prefer `uv run cardputer-codex-middleware --real-codex --prompt "Hello Codex"` for the stable path.
- Use a local WebSocket app-server only for explicit debug setups.
- Regenerate schema files after Codex upgrades if transport messages change.

## Smoke Tests

- Run `uv run python -m unittest discover -s tests -v` in `middleware/`.
- Run `python -m platformio run` in `firmware/`.
- If the build passes but the device is blank, re-check the firmware bin path and the launcher target.

