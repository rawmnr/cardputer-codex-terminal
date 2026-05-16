# Cardputer Firmware

## Role

The firmware turns the Cardputer into a lightweight physical terminal for Codex.

## Deliverable

The primary software artifact must be a flashable `.bin` file that can be launched through M5 Launcher on the Cardputer.

Suggested output name:

```text
cardputer-codex-terminal.bin
```

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

- Status bar: network, battery, Codex state.
- Main area: agent stream and latest middleware status.
- Input line: keyboard prompt.
- Approval screen: requested action, accept/reject.

## Current Firmware Bridge Behavior

- typed prompts can be sent to the middleware bridge;
- push-to-talk audio chunks are streamed while recording;
- release sends a voice prompt ready signal to trigger transcription on Windows;
- streamed Codex deltas, usage, and approval requests are rendered in the app views.

## Constraints

- The 240 x 135 display requires strict wrapping.
- Scrolling must avoid flicker.
- The firmware should not carry complex Codex logic.
- Secrets must be stored and displayed carefully.
- The final firmware output must remain a `.bin`, not a desktop executable or loose script.
