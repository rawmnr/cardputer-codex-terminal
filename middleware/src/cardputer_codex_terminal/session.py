from __future__ import annotations

from dataclasses import dataclass


@dataclass(slots=True)
class CodexSession:
    workspace_path: str = "."
    branch: str | None = None
    thread_id: str | None = None

