# Decision log

- CI middleware job must run `uv run pytest tests -v` from `middleware/`; the old `middleware/tests` path was wrong under the job working directory.
- Middleware docs now standardize on `uv run pytest tests -v`; `python -m unittest` is no longer the canonical command.
- Protocol version stays `2` in middleware, firmware, and fixture contracts; tests now fail if fixture envelopes drift from `PROTOCOL_VERSION`.
- Firmware approval responses must carry the approval ID; native coverage now exercises `sendApprovalResponse(true, "approval-123")`.
- Device-facing projection shape is centralized in `CardputerProjection`; `core.py` delegates session status assembly to the projection layer.
- The duplicate `bleak` dependency entry was removed; pytest collection warnings from `TestSummary` imports were aliased away.
