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


### Multi-page and Interactive Navigation
The preview tool supports sequential actions defined in the fixture JSON. Each action produces a new screenshot frame, so agents can inspect navigation across multiple pages.

**Fixture Actions Example**:
```json
{
  "actions": ["right", "right", "tab", "enter", "A", "esc"]
}
```

**Supported Action Names**:
- `up`, `down`, `left`, `right`
- `enter`, `esc`, `tab`, `backspace`, `del`, `home`, `end`, `prev`
- Single characters (e.g., `"A"`, `"1"`, `"#"`)

Running the preview with such a fixture generates `prefix_0_initial.png`, `prefix_1_action.png`, etc.
## Workflow

### Using Fixtures
Fixtures are JSON files located in `firmware/preview/fixtures/` that define a `DeviceState`. You can generate a preview of a specific state by passing the fixture path to the preview binary:

```bash
.pio/build/preview/program firmware/preview/fixtures/buddy_idle.json preview.png
```

### Agent / MCP Workflow
The tool returns the final frame plus a `frames` array, so agents can inspect each navigation step in order.
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
The preview runner supports simulating keyboard events. `tab` now moves the app-menu focus to the next page when the menu is open, `enter` selects the focused page, and `menu` toggles the app menu.

## Acceptance Testing
The preview system is deterministic. You can compare generated PNGs against "golden" images to detect visual regressions in CI or during development.

## Troubleshooting

### 403 Forbidden Errors (PlatformIO)
If PlatformIO fails to download the `native` platform or libraries with 403 errors, ensure your environment can reach `dl.registry.platformio.org` or use a local mirror/proxy.
