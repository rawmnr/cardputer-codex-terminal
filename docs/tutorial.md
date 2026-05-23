# Dev Quick-Start

Drive your local agent from the Cardputer. This guide covers core interaction loops.

> **Prerequisite**: Ensure device displays `Connected`.

## 1. The First Prompt
1.  Type `Hello Codex`.
2.  Press **Enter**.
3.  Observe agent response on the Cardputer display.

## 2. Agentic Voice Loop (PTT)
1.  **Hold SPACE**: Activates PDM microphone.
2.  **Speak**: e.g., "Implement a LRU cache in `src/utils.py`."
3.  **Release**: Triggers `faster-whisper` transcription on host.

## 3. Approvals (Human-in-the-loop)
When the agent requests a tool call (shell/write):
1.  **Inspect**: View the pending action on screen.
2.  **Act**: `Enter` to approve; `Del` to abort.

## 3. Approving Actions
Sometimes Codex will want to run a command or write a file.
1.  A "Pending Approval" message will appear on the Cardputer.
2.  Press **Enter** to **Accept**.
3.  Press **Del** (Backspace) to **Reject**.

## 4. App Navigation (`Ctrl + M`)
Switch between specialized UI modes:
*   **Push to Codex**: Default chat/voice loop.
*   **Codex Pager**: History inspection and diff review.
*   **Codex Usage**: Live telemetry and rate-limit tracking.


## 5. Advanced Driver Commands
Try these prompts:
*   "Summarize my `git diff`."
*   "Run `pytest` and fix any failures in the auth module."
*   "Map out the dependency graph for `middleware/src`."

---
**Next**: See **[Architecture Guide](architecture.md)** for state-sync details.