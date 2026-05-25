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
