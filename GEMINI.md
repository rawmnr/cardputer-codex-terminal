# Cardputer Codex Terminal: Project Instructions

This project turns the M5Stack Cardputer ADV into a physical terminal for OpenAI Codex running on a Windows host. It consists of a C++/Arduino firmware and a Python middleware service.

## Core Mandates

- **Surgical Updates:** When modifying the firmware, ensure that changes are compatible with the existing `DeviceState` and `MiddlewareLink` abstractions.
- **Protocol Integrity:** The message contract between firmware and middleware is versioned (`PROTOCOL_VERSION = 1`). Any changes to message types in `middleware/src/cardputer_codex_terminal/messages.py` must be mirrored in `firmware/src/middleware_link.cpp/h`.
- **Windows Context:** The middleware is designed for Windows. Use `uv` for Python dependency management.

## Project Structure

- `firmware/`: ESP32-S3 firmware using PlatformIO and M5Unified.
  - `src/main.cpp`: Entry point.
  - `src/apps.cpp`: Application logic (Codex Buddy, Push to Codex, etc.).
  - `src/middleware_link.cpp`: WebSocket communication with the middleware.
  - `src/device_state.h`: Shared state across the firmware.
- `middleware/`: Python-based bridge service.
  - `src/cardputer_codex_terminal/cli.py`: CLI entry point.
  - `src/cardputer_codex_terminal/server.py`: WebSocket server for the firmware.
  - `src/cardputer_codex_terminal/messages.py`: Protocol definitions.
  - `tests/`: Comprehensive test suite for the middleware.
- `docs/`: Detailed architectural and technical documentation.

## Building and Running

### Middleware (Python)
The middleware uses `uv` for environment management.
- **Install dependencies:** `cd middleware && uv sync`
- **Run the service:** `uv run cardputer-codex-middleware --serve`
- **Run tests:** `uv run python -m unittest discover -s tests -v`
- **CLI help:** `uv run cardputer-codex-middleware --help`

### Firmware (C++/PlatformIO)
- **Build firmware:** `cd firmware && python -m platformio run`
- **Upload firmware:** `cd firmware && python -m platformio run --target upload`
- **Monitor serial:** `cd firmware && python -m platformio device monitor`
- **Output binary:** `firmware/cardputer-codex-terminal.bin` (generated after successful build)

## Development Conventions

- **Firmware:**
  - Use `M5Unified` and `M5GFX` for hardware abstraction.
  - Follow the existing `App` interface in `apps.h` for adding new features.
  - State management is centralized in `DeviceState`.
- **Middleware:**
  - Use `dataclass` for message and event objects.
  - Use `websockets` for communication.
  - Maintain high test coverage in `middleware/tests`.
- **Documentation:**
  - Update relevant `.md` files in `docs/` when making architectural changes.
  - Keep the `README.md` updated with the latest build/run instructions.

## Key Technologies
- **Hardware:** M5Stack Cardputer (ESP32-S3).
- **Firmware:** C++, Arduino Framework, PlatformIO, M5Unified, ArduinoJson.
- **Middleware:** Python 3.11+, uv, websockets.
- **Backend:** OpenAI Codex (bridged via middleware).
