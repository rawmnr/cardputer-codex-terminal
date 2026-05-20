# Headless LVGL Preview Workflow

This project includes a headless LVGL preview system that allows developers and coding agents to iterate on the Cardputer UI without flashing the physical hardware.

## Architecture

The preview system uses a native build of the firmware UI code, swapping the M5Stack hardware backend for a headless framebuffer backend.

```text
firmware/src/ui/
  lvgl_port.h               # Unified port interface
  lvgl_port_m5.cpp          # M5Cardputer display backend (Hardware)
  lvgl_port_preview.cpp     # Headless framebuffer backend (Native)
  preview_main.cpp          # Native runner (Fixture loading + PNG export)
```

## Setup

### 1. Host Compiler
You need MSYS2 `g++` (MinGW-w64) installed on your Windows host machine to match the PlatformIO `native` build configuration.

### 2. Build the Preview Binary
Run the following command in the `firmware/` directory:

```bash
pio run -e preview
```

The resulting binary will be located at `.pio/build/preview/program` (or `program.exe`).

## Workflow

### Using Fixtures
Fixtures are JSON files located in `firmware/preview/fixtures/` that define a `DeviceState`. You can generate a preview of a specific state by passing the fixture path to the preview binary:

```bash
.pio/build/preview/program firmware/preview/fixtures/buddy_idle.json preview.png
```

### Agent / MCP Workflow
The middleware exposes a `cardputer.preview_lvgl_ui` MCP tool. This is the preferred way for coding agents to verify UI changes.

**Tool Call Example:**
```json
{
  "name": "cardputer.preview_lvgl_ui",
  "arguments": {
    "screen": "buddy",
    "fixture": "buddy_online_busy",
    "actions": ["down", "enter"]
  }
}
```

**Actions:**
The preview runner supports simulating keyboard events. Supported actions include: `up`, `down`, `left`, `right`, `enter`, `esc`, `backspace`, `home`, `end`.

## Acceptance Testing
The preview system is deterministic. You can compare generated PNGs against "golden" images to detect visual regressions in CI or during development.

## Troubleshooting

### 403 Forbidden Errors (PlatformIO)
If PlatformIO fails to download the `native` platform or libraries with 403 errors, ensure your environment can reach `dl.registry.platformio.org` or use a local mirror/proxy.
