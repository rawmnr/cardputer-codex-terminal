# Testing Strategy

This repository uses a logic-first, hardware-last test stack.

## 1. Firmware native tests

Run the host-side firmware checks without a Cardputer:

```bash
cd firmware
python -m platformio run -e native_tests
```

This builds a native executable that exercises firmware protocol framing, keyboard mapping, and terminal snapshot generation against deterministic fakes.

## 2. Firmware real-target build

Verify the deployable Cardputer firmware still builds:

```bash
cd firmware
python -m platformio run -e cardputer_codex
```

## 3. Preview and golden snapshot tests

Use the native preview build for deterministic UI snapshots:

```bash
cd firmware
python -m platformio run -e preview
```

The preview fixtures under `firmware/preview/fixtures/` and the shared golden fixtures under `tests/contracts/terminal_snapshots/` are used to validate terminal rendering and key UI states.

## 4. Middleware unit and integration tests

Run the host middleware suite:

```bash
cd middleware
uv run pytest tests -v
```

These tests use fake serial bridges and fake Codex transports; no real serial device or Codex call is required.

## 5. Shared protocol contract tests

The shared JSON contracts live under `tests/contracts/`.

- `serial_protocol_cases.json` covers outbound envelope framing and bridge round-trips.
- `keyboard_events.json` covers keyboard mapping cases.
- `terminal_snapshots/` covers deterministic terminal render snapshots.

Both the firmware native runner and the middleware contract tests consume the same fixtures.

## 6. HIL smoke tests

Hardware-in-the-loop checks stay narrow and optional/manual/nightly:

- boot the Cardputer;
- verify the startup screen;
- press one key;
- validate one serial/bridge round-trip;
- confirm a corrupted frame does not crash the device.

Run HIL only after the native and preview layers are green.
