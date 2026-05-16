# Windows Middleware

## Role

The middleware acts as the bridge between the Cardputer and Codex. It compensates for the microcontroller's limits and isolates the Codex protocol from the firmware.

## Responsibilities

- Expose a WebSocket server to the Cardputer.
- Receive text commands.
- Receive PCM audio fragments.
- Transcribe audio to text.
- Open a JSON-RPC session to `codex app-server`.
- Start or resume Codex threads.
- Relay deltas and status updates back to the Cardputer.
- Handle approval requests.

## Target Voice Pipeline

```text
PCM int16 -> memory buffer -> optional VAD -> resample to 16 kHz if needed -> faster-whisper -> text -> turn/start
```

## Constraints

- Non-blocking async loop.
- No persistent audio storage by default.
- Careful logging to avoid writing secrets or sensitive prompts.
- Simulation mode is useful before the real firmware exists.
