# Cardputer Firmware

## Role

The firmware turns the Cardputer into a lightweight physical terminal for Codex.

## Planned Modules

- Wi-Fi management.
- Overlay VPN integration.
- WebSocket client.
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
- Main area: agent stream.
- Input line: keyboard prompt.
- Approval screen: requested action, accept/reject.

## Constraints

- The 240 x 135 display requires strict wrapping.
- Scrolling must avoid flicker.
- The firmware should not carry complex Codex logic.
- Secrets must be stored and displayed carefully.
