# Project Review Findings

This review compares the current repository against the documented product plan: a lightweight M5Stack Cardputer terminal that sends keyboard and voice prompts to a Windows middleware, receives streamed Codex state, surfaces approvals, and manages long-running runs through a pager-style UI.

## Verification snapshot

Commands run during review:

```text
cd middleware
uv run python -m unittest discover -s tests -v
```

Result: failed.

Observed failures:

- `test_shared_prompt_fixture_uses_fake_codex_transport` failed because the expected prompt was never delivered to the fake Codex transport.
- `test_security_hardened` failed to import because it imports `pytest`, but the middleware test command is `unittest` and `pytest` is not declared in `middleware/pyproject.toml`.

Firmware checks were attempted but could not run in this environment:

```text
cd firmware
python -m platformio run -e native_tests
pio --version
```

Both failed because PlatformIO is not installed or not on PATH.

## P0 — Fix before relying on the bridge

### 1. Device RBAC currently blocks required product flows

The middleware WebSocket bridge applies a hardcoded simulated device policy before routing device messages. The allow-list includes only:

```text
status, approve, reject, interrupt, voice_prompt, ping, hello
```

That omits several first-class Cardputer message types:

- `text_prompt`
- `audio_chunk`
- `voice_prompt_ready`
- `run_list_request`
- `run_detail_request`
- `approval_inbox_request`
- `bridge_response`

This directly conflicts with the acceptance criteria in `docs/product-scope.md`: keyboard/voice prompts, streaming replies, pending approvals, and pager-managed long-running tasks.

Evidence:

- `middleware/src/cardputer_codex_terminal/server.py` defines `simulated_device_policy` and rejects actions not in the allow-list.
- `middleware/src/cardputer_codex_terminal/policies.py` implements substring-based `evaluate_request_for_device`.
- The middleware contract test for a text prompt failed: the prompt list remained empty.

Recommended fix:

- Remove the simulated policy from `server.py`.
- Replace it with an explicit bridge capability policy keyed by `CardputerMessageType`, not substring matching.
- Add tests proving that every legitimate firmware-originated message routes successfully when authenticated.
- Add tests proving intentionally dangerous management actions remain rejected.

### 2. Bridge state is sent before authentication

`CardputerBridgeServer.handle_connection()` sends a `bridge_connected` message immediately on WebSocket connect. That message includes local workspace/session metadata:

- `workspace_path`
- `branch`
- `thread_id`

The bridge token check happens later, inside `handle_raw_message()`, after the client sends a message. `hello` also returns `hello_ack` before the token check.

Evidence:

- `middleware/src/cardputer_codex_terminal/server.py` builds `_bridge_connected_message()` with session metadata.
- `handle_connection()` sends that message before any authentication.
- `handle_raw_message()` checks `bridge_token` only after `HELLO` handling.
- `docs/audit_findings.md` already identifies this issue.

Recommended fix:

- Do not send stateful frames until authentication succeeds.
- Make `hello` include the token or introduce a minimal unauthenticated challenge frame.
- Return only non-sensitive metadata before authentication, such as protocol version and required auth mode.
- Add tests for unauthenticated connect and authenticated connect behavior.

### 3. Protocol version has drifted across firmware, middleware, docs, and contracts

Middleware defaults to protocol version `2`, while firmware and multiple docs/fixtures still use protocol version `1`.

Evidence:

- `middleware/src/cardputer_codex_terminal/messages.py`: `PROTOCOL_VERSION = 2`
- `firmware/src/protocol.cpp`: emits `doc["protocol_version"] = 1`
- `tests/contracts/serial_protocol_cases.json`: fixtures use protocol `1`
- `AGENTS.md`, `docs/middleware.md`, `docs/status.md`, and `middleware/README.md` still describe current protocol version `1`

The current parser accepts older versions, so the system may appear to work while the documented contract is wrong.

Recommended fix:

- Decide whether the active protocol is `1` or `2`.
- Update firmware, middleware, shared fixtures, and docs in one change.
- Add a single source of truth for the protocol version in tests, so drift fails quickly.
- If version `2` is intentional, document what changed from version `1`.

### 4. The middleware test stack is inconsistent

The documented middleware command uses `unittest`:

```text
uv run python -m unittest discover -s tests -v
```

But `middleware/tests/test_security_hardened.py` imports `pytest` and uses bare pytest-style functions. Since `pytest` is not declared as a dependency, the standard test command fails at import time.

Evidence:

- `middleware/tests/test_security_hardened.py` imports `pytest`.
- `middleware/pyproject.toml` does not list `pytest`.
- The full unittest run fails with `ModuleNotFoundError: No module named 'pytest'`.

Recommended fix:

- Prefer converting `test_security_hardened.py` to `unittest`, matching the rest of the suite.
- If pytest is desired, add it deliberately and update all test docs/commands.

## P1 — Make implementation more faithful to the plan

### 5. Voice transcription is documented as real, but defaults to mock with no CLI switch

The middleware contains both a mock transcriber and a `FasterWhisperVoiceTranscriber`, but `MiddlewareApp` defaults to `MockVoiceTranscriber`. There is no CLI/config option to select faster-whisper, model size, device, or compute type.

Evidence:

- `middleware/src/cardputer_codex_terminal/core.py` initializes `transcriber` with `MockVoiceTranscriber`.
- `middleware/src/cardputer_codex_terminal/voice.py` defines `FasterWhisperVoiceTranscriber`.
- README and docs describe faster-whisper transcription as part of the middleware role.

Recommended fix:

- Add explicit CLI options, for example:
  - `--voice-transcriber mock|faster-whisper`
  - `--whisper-model tiny|base|small|...`
  - `--whisper-device cpu|cuda`
  - `--whisper-compute-type int8|float16|...`
- Keep mock as default for tests/local scaffolding, but make docs clear about that default.
- Add tests for transcriber selection and voice prompt finalization.

### 6. Worker run orchestration starts a thread but does not submit work

`/run worker` creates an `AgentRun` and starts or resumes a Codex thread through `CodexProcessSupervisor`, but no task prompt is submitted to Codex. That means the pager can show a run object without an actual agent turn doing work.

Evidence:

- `middleware/src/cardputer_codex_terminal/commands.py` creates runs in `start_worker()` and calls `supervisor.start_run(run)`.
- `middleware/src/cardputer_codex_terminal/supervisor.py` starts/resumes a thread, but does not call `start_turn()`.

Recommended fix:

- Require a task prompt when starting a worker run.
- Or rename the command/UI to make it clear it only creates/selects a run session.
- Model run lifecycle around explicit states: queued, started, turn running, waiting approval, done/failed.

### 7. Firmware advertises mDNS auto-discovery but does not implement it

The middleware registers `_cardputer-codex._tcp.local` via zeroconf, and firmware logs that it is auto-discovering the bridge when no host is configured. However, the firmware does not query mDNS for that service.

Evidence:

- `middleware/src/cardputer_codex_terminal/cli.py` registers `_cardputer-codex._tcp.local`.
- `firmware/src/app_shell.cpp` logs `Auto-discovering bridge...` when no middleware host is configured.
- The firmware includes `ESPmDNS.h`, but no service query logic is present.

Recommended fix:

- Implement mDNS discovery on firmware, including timeout/fallback behavior.
- Or remove the auto-discovery messaging and require explicit `middleware_host` until discovery exists.
- Update `firmware/config.example.ini` and docs to reflect the real behavior.

### 8. Approval responses can drop the approval id

`AppShell::handleApprovalDecision()` clears `state_.approval_id` and then calls `sendApprovalResponse(approved)` without passing the id. The middleware then has to infer the pending approval from current session state.

Evidence:

- `firmware/src/app_shell.cpp` clears approval fields before sending the response.
- `firmware/src/middleware_link.cpp` supports sending an approval id, but the caller does not pass one.

Recommended fix:

- Capture the current approval id before clearing UI state.
- Call `sendApprovalResponse(approved, approval_id)`.
- Add a native firmware test for approval response envelopes.

### 9. Voice target cycling appears unreachable

`PushToCodexApp::onAction()` cycles `voice_target` when it receives `UiAction::None`. But `AppShell::handleAction()` immediately returns when action is `None`, so this branch cannot be reached through the normal shell action path.

Evidence:

- `firmware/src/apps.cpp` handles voice target cycling under `UiAction::None`.
- `firmware/src/app_shell.cpp` returns immediately on `UiAction::None`.

Recommended fix:

- Assign target cycling to a real key/action.
- Or remove target cycling until the UX has a real input path.
- Add a keyboard/action contract test for voice intent and target changes.

## P2 — Simplify and harden maintainability

### 10. There are too many overlapping state projection shapes

Current state flows through multiple manually maintained shapes:

- full `session_status` payloads in `core.py`
- compact projections in `projections.py`
- firmware parsing in `middleware_link.cpp`
- preview mirror state in `preview.py`
- shared JSON fixtures under `tests/contracts/`

This increases drift risk and makes protocol changes expensive.

Recommended fix:

- Make `CardputerProjection` the only source for device-facing snapshots.
- Route `status_request`, run list/detail, and approval inbox responses through projection builders.
- Keep full desktop preview-only state separate from firmware-bound compact state.
- Add contract tests generated from projection outputs.

### 11. `server.py` mixes transport framing, authentication, policy, routing, and serialization

The bridge server currently handles:

- WebSocket lifecycle
- event subscription
- ack formatting
- connection banner formatting
- JSON parsing
- token validation
- simulated RBAC
- app routing
- exception-to-ack behavior

This makes security bugs easier to introduce.

Recommended fix:

- Extract `BridgeCodec` for ack/event/envelope serialization.
- Extract `BridgeAuth` for token and future pairing checks.
- Extract `DeviceCapabilityPolicy` for per-message authorization.
- Keep `CardputerBridgeServer` as orchestration glue.

### 12. Firmware audio chunking allocates per chunk

`MiddlewareLink::sendAudioChunk()` allocates a new base64 buffer with `std::unique_ptr` and creates a JSON document for every chunk. Voice capture is one of the highest-frequency firmware paths.

Evidence:

- `firmware/src/middleware_link.cpp` allocates encoded audio per chunk.

Recommended fix:

- Reuse fixed-size buffers owned by `PushToCodexApp` or `MiddlewareLink`.
- Keep maximum chunk sizes explicit and test them against `WEBSOCKETS_MAX_DATA_SIZE` and ArduinoJson capacity.
- Consider a binary side channel later, but fixed-buffer JSON/base64 is the simplest near-term improvement.

### 13. Large files should be split by responsibility

Several files are carrying multiple subsystems:

- `firmware/src/app_shell.cpp`
- `firmware/src/apps.cpp`
- `firmware/src/middleware_link.cpp`
- `middleware/src/cardputer_codex_terminal/preview.py`
- `middleware/src/cardputer_codex_terminal/core.py`

Recommended fix:

- Split by stable boundaries, not by arbitrary size:
  - shell input/action handling
  - bridge event ingestion
  - app implementations
  - preview HTTP API
  - preview static HTML/template
  - middleware app orchestration vs device prompt handling

## Documentation corrections

### README

The README currently presents faster-whisper transcription as part of the working middleware path. It should distinguish implemented defaults from optional/planned real transcription.

Recommended wording direction:

- Mock transcription is the current default.
- faster-whisper support exists in code but needs explicit wiring/config before it is the normal runtime path.

### `docs/middleware.md`

Update protocol version and clarify which transport/voice features are complete vs planned.

### `docs/firmware.md`

Clarify mDNS discovery status and align keymap with actual reachable actions.

### `docs/status.md`

This file is stale in several places:

- protocol version says `1`
- transcription status says local models are not delivered, while README implies transcription is ready
- test/build verification notes should mention current local blockers if kept as a living status doc

### `docs/audit_findings.md`

The security findings in this file are still relevant. Either merge them into this review/roadmap or turn them into tracked tasks. Leaving known bridge-auth issues as passive notes makes them easy to miss.

## Suggested implementation order

1. Repair bridge authorization so legitimate firmware messages route.
2. Move authentication before any stateful bridge response.
3. Normalize protocol version across middleware, firmware, docs, and contract fixtures.
4. Make the standard middleware test command green.
5. Fix approval id propagation.
6. Clarify/implement real voice transcription selection.
7. Decide whether mDNS discovery is implemented now or documented as future work.
8. Simplify projection and server boundaries once behavior is correct.
## Existing GitHub issues reviewed

These open issues already cover part of the current problem set and should be kept distinct from the new findings below:

- #26 / #30: bridge token validation happens too late and leaks session metadata.
- #27: port conflict handling is ungraceful.
- #28: token exposure through CLI arguments.
- #29: raw JSON interleaves with terminal output.

## Proposed new issues

1. **[Security] Bridge RBAC blocks legitimate Cardputer messages**
   - Evidence: `server.py` allows only a small simulated action set and rejects `text_prompt`, `audio_chunk`, `voice_prompt_ready`, `run_list_request`, `run_detail_request`, `approval_inbox_request`, and `bridge_response`.
   - Why new: this is distinct from token leakage; it prevents the core product loop from working.
   - Acceptance: authenticated device-originated prompt, voice, pager, and approval messages route successfully.

2. **[Protocol] Firmware/middleware protocol version drift**
   - Evidence: middleware defaults to protocol `2`, while firmware, fixtures, and docs still describe `1`.
   - Why new: this is a contract mismatch, not a generic docs issue.
   - Acceptance: one source of truth for protocol version across firmware, middleware, fixtures, and docs.

3. **[Test] Middleware test suite mixes unittest and pytest**
   - Evidence: `test_security_hardened.py` imports `pytest`, but the documented command is `unittest` and `pytest` is not a dependency.
   - Why new: this blocks the standard test command.
   - Acceptance: `uv run python -m unittest discover -s tests -v` passes without extra test dependencies.

4. **[Voice] Transcription backend selection needs explicit wiring**
   - Evidence: `FasterWhisperVoiceTranscriber` exists, but the middleware defaults to mock transcription and exposes no CLI/config switch.
   - Why new: implementation exists but the real runtime path is not selectable.
   - Acceptance: CLI/config can select mock vs faster-whisper and its model/device settings.

5. **[Runs] Worker run startup should submit actual work**
   - Evidence: `/run worker` creates a run and thread but never submits a task prompt to Codex.
   - Why new: pager/session plumbing exists, but worker execution does not begin.
   - Acceptance: starting a worker run either submits a task prompt or is clearly named as session creation only.

6. **[Firmware] mDNS auto-discovery is advertised but missing**
   - Evidence: firmware says it is auto-discovering the bridge, but no mDNS query path exists.
   - Why new: this is a missing feature promised by the current UI/log messaging.
   - Acceptance: firmware can resolve the bridge without a hardcoded host, or the docs stop promising discovery.

7. **[Firmware] Approval responses should carry the approval id**
   - Evidence: approval state is cleared before the response is sent, so the middleware cannot reliably correlate the reply.
   - Why new: this is an end-to-end correctness issue for approvals.
   - Acceptance: approval response envelopes always include the pending approval id.

8. **[Firmware] Voice target cycling is unreachable**
   - Evidence: the app cycles target on `UiAction::None`, but the shell drops `None` before dispatching to apps.
   - Why new: this is dead code plus a UX gap.
   - Acceptance: target cycling is reachable through a real input action or removed.

9. **[Design] Projection shapes should be centralized**
   - Evidence: status/session/run/approval shapes are duplicated across middleware, firmware, preview, and fixtures.
   - Why new: this is a maintainability issue, not a single bug.
   - Acceptance: device-facing snapshots come from one projection layer and shared fixtures validate it.
