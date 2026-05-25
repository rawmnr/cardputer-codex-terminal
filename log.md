# Decision log

- CI middleware job must run `uv run pytest tests -v` from `middleware/`; the old `middleware/tests` path was wrong under the job working directory.
- Middleware docs now standardize on `uv run pytest tests -v`; `python -m unittest` is no longer the canonical command.
- Protocol version stays `2` in middleware, firmware, and fixture contracts; tests now fail if fixture envelopes drift from `PROTOCOL_VERSION`.
- Firmware approval responses must carry the approval ID; native coverage now exercises `sendApprovalResponse(true, "approval-123")`.
- Device-facing projection shape is centralized in `CardputerProjection`; `core.py` delegates session status assembly to the projection layer.
- The duplicate `bleak` dependency entry was removed; pytest collection warnings from `TestSummary` imports were aliased away.
- Repository guidance now requires reading `LOG.md` before refactors, updating it after meaningful decisions, updating it again before commits, and replacing stale decisions instead of letting them accumulate.
- Phase B firmware split uses a UI/background task boundary with a bounded in-memory event queue for keyboard-driven actions; the queue is a ring buffer to keep native/preview builds simple.
- SPI/I2C access is guarded with a small scoped lock helper; SD logging and LVGL flush now take the SPI guard instead of touching shared bus state directly.
- Phase B finalized: runtime task isolation, I2C/SPI resource protection, queue overflow policy, and BLE callback safety using AppEvents. All tests passed.

- Phase C UI/performance refactor chose retained-mode LVGL double buffering on hardware, with native preview mirroring and a polling fallback for keyboard IRQ because the verified interrupt pin is still unknown. Runtime diagnostics now surface display flush, keyboard, and bus metrics in Settings.
- `FirmwareRuntime` now exposes a narrow `enqueueEvent()` wrapper for cross-task event injection; BLE callbacks use that instead of touching the queue directly.
- Phase D simulator now builds against SDL3, not SDL2. The Windows host package is `SDL3-devel-3.4.8-mingw`; the build script copies `libSDL3.dll.a` into the repo-local MinGW search path and drops `SDL3.dll` beside the simulator binary so the regression loop can run without manual linker setup.
- `tests/ui/scenarios/terminal_mock_response.json` now drives the host simulator through event-style network states (`wifi_connecting`, `codex_request_started`, `token_chunk`, etc.) so the screenshot loop covers streamed response UI, not just typed commands.
