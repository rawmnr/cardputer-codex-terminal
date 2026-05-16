from __future__ import annotations

from dataclasses import dataclass


@dataclass(slots=True)
class AppConfig:
    host: str = "127.0.0.1"
    port: int = 8765
    codex_ws_url: str = "ws://127.0.0.1:9000"
    use_mock_codex: bool = True
    bridge_token: str | None = None
    workspace_path: str = "."
    branch: str | None = None
    thread_id: str | None = None
