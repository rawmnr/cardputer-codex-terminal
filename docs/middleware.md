# Windows Middleware

## Role

The middleware acts as the bridge between the Cardputer and Codex. It compensates for the microcontroller's limits and isolates the Codex protocol from the firmware.

The Python package in `middleware/` is managed with `uv`:

```bash
uv sync
uv run cardputer-codex-middleware
uv run python -m unittest discover -s tests -v
```

Cardputer-facing messages use a versioned envelope. The current protocol version is `1`.
When the bridge is exposed beyond loopback, the server can require a shared `bridge_token` and the firmware must embed the same token in its outgoing envelopes.

## Responsibilities

- Expose a WebSocket server to the Cardputer.
- Receive text commands.
- Receive PCM audio fragments.
- Transcribe audio to text.
- Open a JSON-RPC session to `codex app-server`.
- Start or resume Codex threads.
- Relay deltas and status updates back to the Cardputer.
- Handle approval requests.
- Handle local bridge notifications, questions, confirmations, and responses.

The middleware CLI supports a `--serve` mode that listens for versioned Cardputer messages over WebSocket and turns them into middleware events.
Use `--bridge-token` to require a shared secret for the Cardputer bridge.

The voice pipeline is intentionally split into three steps:

1. collect PCM chunks from the Cardputer;
2. buffer them until the user releases push-to-talk;
3. transcribe the buffered audio into a prompt before handing it to Codex.

## Target Voice Pipeline

```text
PCM int16 -> memory buffer -> optional VAD -> resample to 16 kHz if needed -> faster-whisper -> text -> turn/start
```

## Constraints

- Non-blocking async loop.
- No persistent audio storage by default.
- Careful logging to avoid writing secrets or sensitive prompts.
- Simulation mode is useful before the real firmware exists.
