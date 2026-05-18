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

Phase 1 of the LVGL migration uses two PlatformIO environments:

- `cardputer_codex` builds the LVGL-backed proof-of-concept UI.
- `cardputer_codex_legacy` keeps the existing text renderer as the fallback.

The active path is controlled by the `USE_LVGL_UI` build flag.

The shell remains the source of truth for app and menu state; the LVGL layer mirrors that state and keeps focus/selection visible without doing a second, conflicting state transition.
The display lifecycle now uses active, dimmed, and low-power brightness levels, and any key press restores a safe visible brightness instead of dropping the panel to black.
Approval requests and bridge prompts are rendered as LVGL modals with a shared dialog widget; notification prompts can be dismissed with `Enter` or `Del`.
Push-to-talk now has a dedicated LVGL recording panel that shows armed, recording, ready, and error states along with the peak level and capture progress details.
Phase 6 starts the per-app LVGL screen split with an `LvglAppScreen` interface and a Buddy dashboard screen; the legacy text renderer remains the fallback path for the other apps during the migration.

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

## User Interface

- App selection is menu-driven, opened with `Ctrl-M`, not a persistent tab bar.
- The shell chrome uses a slim dark header with status dots, a color-coded Codex pill, and a footer that switches between hints and the command palette input line.
- The app menu is an overlay list, while app views stay compact and avoid repeating global status that already lives in the chrome.
- Global navigation: `Ctrl-M` opens the app menu, `Fn+; / Fn+.` move selection, `Tab` opens the menu or advances LVGL focus, `Enter` select/approve, `Del` back/reject.
- Contextual footer hints replace the old shell-first help as the primary on-device guide.
- Command palette remains available through `/`, but it is now the debug layer rather than the default flow.
- Approval screen: requested action, accept/reject.
- Pager screen: compose prompts, browse active/recent Codex sessions, and inspect recent events.

## Current Firmware Bridge Behavior

- typed prompts can be sent to the middleware bridge from Push to Codex or Pager compose mode;
- push-to-talk audio chunks are streamed while recording;
- release sends a voice prompt ready signal to trigger transcription on Windows;
- streamed Codex deltas, usage, and approval requests are rendered in the app views;
- the Pager app consumes the middleware session index and renders COMPOSE, INBOX, and DETAIL views from that Python-owned model;
- reply selection stays on the Cardputer, but the session history and event stream stay in middleware;
- the MCP bridge app can show notifications, questions, confirmations, and selection state;
- the Cardputer can accept or reject pending prompts with physical keys;
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

## Constraints

- The 240 x 135 display requires strict wrapping.
- Scrolling must avoid flicker.
- The firmware should not carry complex Codex logic.
- Secrets must be stored and displayed carefully.
- The final firmware output must remain a `.bin`, not a desktop executable or loose script.
