# Cardputer Firmware

## Role

The firmware turns the Cardputer into a lightweight physical terminal for Codex.

## Deliverable

The primary software artifact must be a flashable `.bin` file that can be launched through M5 Launcher on the Cardputer.

Suggested output name:

```text
cardputer-codex-terminal.bin
```

## UI Backends

Phase C uses three build targets:

- `cardputer_codex` builds hardware with LVGL enabled.
- `preview` runs the native LVGL preview with a mirrored framebuffer.
- `native_tests` keeps logic tests on the text path.

`USE_LVGL_UI` selects the runtime UI backend. Hardware runs retained-mode LVGL screens; preview mirrors the same screen tree; text mode remains the fallback path for non-UI tests.

The shell stays source of truth for app/menu state. LVGL mirrors that state, keeps focus/selection visible, and never performs a second, conflicting transition.
The display lifecycle uses active, dimmed, and low-power brightness levels, and any key press restores visible brightness instead of blacking the panel out.
Approval requests and bridge prompts are LVGL modals with shared dialog widgets; notification prompts dismiss with `Enter` or `Del`.
Push-to-talk has a dedicated LVGL recording panel that shows armed, recording, ready, and error states plus peak level and capture progress.
Each app owns its retained-mode screen via `LvglAppScreen`; Buddy, Push, Pager, Usage, MCP Bridge, and Settings each own their content area.

## Packaging Assumptions

- The firmware source tree must be set up so the build pipeline produces a single installable binary.
- The binary should be suitable for M5 Launcher without extra manual repackaging.
- Any assets or configuration needed at runtime should be bundled or embedded in the build, not left as separate manual steps unless explicitly required.

## Planned Modules

- Wi-Fi management.
- Overlay VPN integration.
- WebSocket client to the Windows middleware.
- Keyboard capture.
- I2S/PDM audio capture.
- Terminal display rendering.
- Sound alerts.
- Battery and sleep management.

## Target FreeRTOS Tasks

| Task | Responsibility |
| --- | --- |
| network_task | Connection, WebSocket, VPN |
| input_task | Keyboard and shortcuts |
| audio_task | Microphone DMA and push-to-talk |
| display_task | Text rendering, status, approvals |
| power_task | Battery, sleep, CPU frequency |

## Phase C Runtime Ownership

- `ui_task`: owns `AppShell`, rendering, LVGL, and all visible state transitions.
- `network_task`: owns Wi-Fi bring-up, bridge polling, and background Codex transport ticks.
- `keyboard_task`: owns I2C keyboard access, IRQ wakeups when available, and bounded event posting for the UI task.
- `ui_task` drains events and applies app actions/text input; queue overflow drops the newest low-priority event and increments diagnostics.

## Resource Ownership

| Resource | Owner | Guard |
| --- | --- | --- |
| LVGL/display state | `ui_task` | implicit UI-thread ownership |
| SPI/SD writes | `network_task` and log/config helpers | `ResourceGuard::Kind::Spi` |
| I2C keyboard polling | `keyboard_task` | `ResourceGuard::Kind::I2c` |
| Bridge transport state | `network_task` | runtime shell mutex while mutating shell state |

## Diagnostics

- Runtime metrics log free heap, queue drops, SPI/I2C contention, keyboard IRQ/poll counts, display flush timings, and task stack high-water marks.
- SD logging uses the SPI guard, so SD writes no longer happen inline with rendering.
- The LVGL flush path now takes the SPI guard so display updates fail closed instead of colliding with SD access.
## User Interface

- App selection is menu-driven, opened with `Ctrl-M`, not a persistent tab bar.
- The shell chrome uses a slim dark header with status dots, a color-coded Codex pill, and a footer that switches between hints and the command palette input line.
- The app menu is an overlay list, while app views stay compact and avoid repeating global status that already lives in the chrome.
- Global navigation: `Ctrl-M` opens the app menu, `Fn+; / Fn+.` move selection, `Tab` opens the menu or advances LVGL focus, `Enter` select/approve, `Del` back/reject.
- Contextual footer hints replace the old shell-first help as the primary on-device guide.
- Command palette remains available through `/`, but it is now the debug layer rather than the default flow.
- Runs and Approvals now live in the main menu, with compact list/detail views on the 240x135 display.
- Run detail can switch to compact diff and test sub-screens for decision making.
- Approval inbox shows multiple pending approvals and uses a double-confirm gesture for dangerous actions.

- Pager screen: compose prompts, browse active/recent Codex sessions, and inspect recent events.

## Current Firmware Bridge Behavior

- typed prompts can be sent to the middleware bridge from Push to Codex or Pager compose mode;
- push-to-talk audio chunks are streamed while recording;
- release sends a voice prompt ready signal to trigger transcription on Windows;
- streamed Codex deltas, usage, and approval requests are rendered in the app views;
The Pager app consumes the middleware session index and renders COMPOSE, INBOX, and DETAIL views from that Python-owned model;
The Runs app consumes the middleware `run_list` and `run_detail` snapshots, while Approvals consumes `approval_inbox`.
The Runs detail screen still shows branch, mode, step, diff, tests, and merge readiness, and now forwards `pause_run`, `resume_run`, `collect_diff`, `run_tests`, `generate_merge_report`, and `mark_for_merge` back to middleware through `/run` actions.
reply selection stays on the Cardputer, but the session history and event stream stay in middleware;

- The Approvals screen lists run, action, and danger level, and rejects remain immediate with `Del`.

- The MCP bridge app can show notifications, questions, confirmations, and selection state;
- The Cardputer can accept or reject pending prompts with physical keys;


- the top-level menu bar is menu-first, while slash commands remain available as the debug path;
- `UiAction` sits between physical keyboard input and app behavior so apps receive navigation, select, back, and push-to-talk events instead of raw keycodes;
- Cardputer bridge envelopes now include a request `id`, and middleware sends back an `ack` before the semantic response;
- Wi-Fi and middleware settings can be loaded from `/cardputer-codex/config.ini` on the SD card;
- the middleware host in that SD config must point at the Windows machine, LAN IP, hostname, or VPN address, not `127.0.0.1`;
- the firmware can create `/cardputer-codex/` and seed a template config file without overwriting an existing one;
- the firmware can append a persistent log to `/cardputer-codex/log.txt` for offline debugging from the SD card.

## Keymap

- `A` / `D`: previous / next tab.
- `,` / `.`: previous / next tab.
- `;` / `'`: previous / next tab.
- `W` / `S`: move up / down in lists and prompts.
- `Tab`: open the app menu, or advance LVGL focus when the menu is already open.
- `Enter`: select, confirm, or submit typed input.
- `Del`: back, reject, or clear text one character at a time while editing.
- `Space` tap: contextual action or literal space while editing text.
- `Space` hold: push-to-talk recording in Push to Codex.
- `/`: open the command palette / debug shell.

## Connectivity & Transports (Devs)

### Wi-Fi Configuration
Edit `/cardputer-codex/config.ini` on the SD card:
```ini
wifi_ssid=YourSSID
wifi_password=YourPassword
bridge_transport=wifi
```

### BLE Configuration
To use Bluetooth instead of Wi-Fi:
1. Set `bridge_transport=ble` in `config.ini`.
2. Set `ble_enabled=true`.
3. The device will advertise as `CardputerCodex` (configurable via `ble_name`).

### Hybrid Mode
To listen for both Wi-Fi and BLE connections:
`bridge_transport=hybrid`

---

## Constraints

- Scrolling must avoid flicker.
- The firmware should not carry complex Codex logic.
- Secrets must be stored and displayed carefully.
- The final firmware output must remain a `.bin`, not a desktop executable or loose script.
