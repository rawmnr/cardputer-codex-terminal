from __future__ import annotations

import contextlib
from dataclasses import dataclass, field
from typing import Any, Callable

from .codex_transport import CodexTransport, StdioCodexAppServerTransport
from .runs import AgentRun, RunMode, RunStatus


@dataclass(slots=True)
class CodexHandle:
    run_id: str
    transport: CodexTransport
    cwd: str
    dedicated: bool
    thread_id: str | None = None
    state: str = "running"
    last_error: str = ""


@dataclass(slots=True)
class CodexProcessSupervisor:
    shared_transport: CodexTransport
    codex_command: tuple[str, ...]
    default_workspace: str
    handle_factory: Callable[[AgentRun, CodexTransport, str, bool], CodexHandle] | None = None
    transport_factory: Callable[[tuple[str, ...], str], CodexTransport] | None = None
    _handles: dict[str, CodexHandle] = field(default_factory=dict, init=False, repr=False)

    def _create_transport(self, cwd: str) -> CodexTransport:
        factory = self.transport_factory or (lambda command, worktree: StdioCodexAppServerTransport(list(command), cwd=worktree))
        return factory(self.codex_command, cwd)

    def _create_handle(self, run: AgentRun, transport: CodexTransport, cwd: str, dedicated: bool) -> CodexHandle:
        factory = self.handle_factory
        if factory is not None:
            return factory(run, transport, cwd, dedicated)
        return CodexHandle(run_id=run.run_id, transport=transport, cwd=cwd, dedicated=dedicated, thread_id=run.thread_id)

    def _workspace_for(self, run: AgentRun) -> str:
        return run.worktree_path or run.workspace_path or self.default_workspace

    async def start_run(self, run: AgentRun) -> CodexHandle:
        handle = self._handles.get(run.run_id)
        if handle is not None:
            run.status = RunStatus.RUNNING
            handle.state = "running"
            handle.last_error = ""
            return handle

        cwd = self._workspace_for(run)
        dedicated = run.mode == RunMode.YOLO_WORKTREE
        transport = self._create_transport(cwd) if dedicated else self.shared_transport
        await transport.initialize()

        if run.thread_id:
            thread_id = await transport.resume_thread(run.thread_id)
        else:
            thread_id = await transport.start_thread(cwd, run.branch)

        if run.branch is not None:
            with contextlib.suppress(Exception):
                await transport.update_thread_metadata(thread_id, branch=run.branch)

        run.thread_id = thread_id
        run.status = RunStatus.RUNNING
        handle = self._create_handle(run, transport, cwd, dedicated)
        handle.thread_id = thread_id
        self._handles[run.run_id] = handle
        return handle

    async def interrupt(self, run_id: str) -> None:
        handle = self._handles.get(run_id)
        if handle is None or handle.thread_id is None:
            return
        with contextlib.suppress(Exception):
            await handle.transport.interrupt_turn(handle.thread_id)
        handle.state = "interrupted"

    async def kill(self, run_id: str) -> None:
        handle = self._handles.get(run_id)
        if handle is None:
            return
        if handle.dedicated:
            close = getattr(handle.transport, "close", None)
            if callable(close):
                with contextlib.suppress(Exception):
                    await close()
            handle.state = "closed"
            self._handles.pop(run_id, None)
            return
        await self.interrupt(run_id)

    async def restart(self, run: AgentRun) -> CodexHandle:
        await self.kill(run.run_id)
        run.thread_id = None
        return await self.start_run(run)

    async def close(self) -> None:
        for run_id in list(self._handles):
            handle = self._handles.pop(run_id)
            if not handle.dedicated:
                continue
            close = getattr(handle.transport, "close", None)
            if callable(close):
                with contextlib.suppress(Exception):
                    await close()
            handle.state = "closed"

    def get_handle(self, run_id: str) -> CodexHandle | None:
        return self._handles.get(run_id)
