# Agentic Workflows with Worktrees

This guide explains how to use the Cardputer as a hardware supervisor for high-stakes, multi-file agentic tasks using **Git Worktrees** and the **Codex CLI**.

## 🏗 The Scenario
You need to perform a complex refactor (e.g., "Port Auth to OAuth2") without touching your current uncommitted changes in your main directory.

---

## 🛠 Step-by-Step Workflow

### 1. Initialize Supervisor Mode
Launch the middleware on your PC, pointing it to your project and enabling the agent supervisor.

```bash
uv run cardputer-codex-middleware --serve --real-codex --workspace ./my-project
```

### 2. Dispatch a Worktree Task (Voice)
Hold **SPACE** on the Cardputer and provide a high-level intent:
> "Create a worktree for `feature/oauth`. Port `auth.py` to the new library and run the unit tests. Do not merge until I review."

### 3. Agentic Isolation
The **Codex App-Server** (via Middleware) will:
1.  Execute `git worktree add ../feature-oauth`.
2.  Root a new agent process in that directory.
3.  Begin editing files.

### 4. Hardware Supervision (The Pager)
As the agent works, your main screen stays on your current task. You monitor progress on the Cardputer:
-   **Buddy Screen**: Shows the active branch (`feature/oauth`) and real-time token usage.
-   **Pager Screen**: You'll see a stream of "Inbox" events like `Worktree Created`, `File Modified: auth.py`, and `Test Run Started`.

### 5. Physical Approvals
When the agent reaches a critical point (e.g., deleting old credentials):
1.  The Cardputer screen flashes **PENDING APPROVAL**.
2.  Press **Enter** to Accept or **Del** to Reject.
3.  The agent continues based on your physical input.

### 6. Review & Merge
Once the tests pass:
1.  Inspect the **Diff Summary** on the Cardputer Pager.
2.  If satisfied, type `/merge` or `/run merge` on the Cardputer.
3.  The supervisor merges the worktree and cleans up.

---

## 💡 Why use Worktrees?
*   **Isolation**: The agent cannot mess up your current `HEAD`.
*   **Parallelism**: You can keep working on your main branch while the agent grinds in the background.
*   **Safety**: If the agent goes "rogue," you simply delete the worktree directory without affecting your core repo.
