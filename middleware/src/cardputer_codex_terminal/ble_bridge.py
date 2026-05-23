from __future__ import annotations

import asyncio
import contextlib
import json
import sys
from collections.abc import Callable, Mapping
from dataclasses import dataclass, field
from typing import Any

try:  # pragma: no cover - exercised indirectly in environments with bleak installed
    from bleak import BleakClient, BleakScanner
except ImportError:  # pragma: no cover - import guard keeps Wi-Fi-only paths usable in lean envs
    BleakClient = None
    BleakScanner = None
    HAS_BLEAK = False
else:  # pragma: no cover - exercised indirectly in environments with bleak installed
    HAS_BLEAK = True

from .bus import EventFilter
from .core import MiddlewareApp
from .events import Event, EventType
from .server import CardputerBridgeServer

SERVICE_UUID = "a5cd0001-c0de-4abe-9c1a-4d5e6f7a8b90"
RX_UUID = "a5cd0002-c0de-4abe-9c1a-4d5e6f7a8b90"
TX_UUID = "a5cd0003-c0de-4abe-9c1a-4d5e6f7a8b90"
DEFAULT_NAME_PREFIX = "CardputerCodex_"
BLE_FORWARD_KINDS = {
    "bridge_connected",
    "bridge_notification",
    "bridge_question",
    "bridge_confirmation",
    "bridge_response",
    "voice_intent_updated",
    "status",
    "status_snapshot",
    "router",
    "heartbeat",
}
BLE_FORWARD_TYPES = {
    EventType.APPROVAL_REQUEST,
    EventType.APPROVAL_RESPONSE,
    EventType.ERROR,
    EventType.CODEX_STATUS,
    EventType.STATUS_SNAPSHOT,
}


def _stderr_logger(message: str) -> None:
    print(message, file=sys.stderr, flush=True)


@dataclass(slots=True)
class CardputerBleBridge:
    app: MiddlewareApp
    name_prefix: str = DEFAULT_NAME_PREFIX
    scan_timeout_s: float = 5.0
    reconnect_delay_s: float = 3.0
    chunk_size: int = 20
    max_event_bytes: int = 512
    logger: Callable[[str], None] = field(default=_stderr_logger, repr=False, compare=False)
    scanner: Any | None = field(default=None, repr=False, compare=False)
    client_factory: Callable[[Any], Any] | None = field(default=None, repr=False, compare=False)
    _server: CardputerBridgeServer = field(init=False, repr=False, compare=False)
    _pending_inbound: set[asyncio.Task[None]] = field(default_factory=set, init=False, repr=False, compare=False)

    def __post_init__(self) -> None:
        if self.chunk_size <= 0:
            raise ValueError("chunk_size must be positive")
        self._server = CardputerBridgeServer(self.app)

    def _log(self, message: str) -> None:
        self.logger(message)

    @staticmethod
    def _normalise_discovery(discovered: Any) -> list[tuple[Any, Any | None]]:
        if isinstance(discovered, Mapping):
            items: list[tuple[Any, Any | None]] = []
            for key, value in discovered.items():
                if isinstance(value, tuple) and len(value) == 2:
                    items.append((value[0], value[1]))
                else:
                    items.append((key, value))
            return items
        items = []
        for entry in discovered or []:
            if isinstance(entry, tuple) and len(entry) == 2:
                items.append((entry[0], entry[1]))
            else:
                items.append((entry, None))
        return items

    def _device_name(self, device: Any, advertisement: Any | None) -> str:
        name = getattr(device, "name", None) or getattr(advertisement, "local_name", None) or ""
        return str(name)

    def _service_uuids(self, advertisement: Any | None) -> set[str]:
        if advertisement is None:
            return set()
        service_uuids = getattr(advertisement, "service_uuids", None) or []
        return {str(uuid).lower() for uuid in service_uuids}

    def _matches_device(self, device: Any, advertisement: Any | None) -> bool:
        name = self._device_name(device, advertisement)
        if name.startswith(self.name_prefix):
            return True
        return SERVICE_UUID in self._service_uuids(advertisement)

    def select_device(self, discovered: Any) -> Any | None:
        for device, advertisement in self._normalise_discovery(discovered):
            if self._matches_device(device, advertisement):
                return device
        return None

    async def _discover_device(self) -> Any | None:
        scanner = self.scanner if self.scanner is not None else BleakScanner
        if scanner is None:
            raise RuntimeError("bleak is required to use the BLE bridge.")
        try:
            discovered = await scanner.discover(timeout=self.scan_timeout_s, return_adv=True)
        except TypeError:
            discovered = await scanner.discover(timeout=self.scan_timeout_s)
        return self.select_device(discovered)

    def _frame_line(self, line: str) -> list[bytes]:
        payload = f"{line}\n".encode("utf-8")
        return [payload[index : index + self.chunk_size] for index in range(0, len(payload), self.chunk_size)]

    def _should_forward_event(self, event: Event) -> bool:
        if event.type not in BLE_FORWARD_TYPES:
            return False
        if event.type in {EventType.APPROVAL_REQUEST, EventType.APPROVAL_RESPONSE, EventType.ERROR}:
            return True
        encoded = json.dumps(event.to_dict(), ensure_ascii=False).encode("utf-8")
        if len(encoded) > self.max_event_bytes:
            return False
        if event.type == EventType.STATUS_SNAPSHOT:
            return True
        kind = str(event.payload.get("kind") or "")
        return kind in BLE_FORWARD_KINDS

    async def _process_incoming_line(self, line: str, outbound: asyncio.Queue[str]) -> None:
        try:
            responses = await self._server.handle_raw_message(line)
        except Exception as exc:
            self._log(f"BLE bridge dropped malformed line: {exc}")
            return
        for response in responses:
            await outbound.put(response)

    def _schedule_incoming_line(self, line: str, outbound: asyncio.Queue[str]) -> None:
        task = asyncio.create_task(self._process_incoming_line(line, outbound))
        self._pending_inbound.add(task)
        task.add_done_callback(self._pending_inbound.discard)
        task.add_done_callback(self._log_task_exception)

    def _log_task_exception(self, task: asyncio.Task[None]) -> None:
        with contextlib.suppress(asyncio.CancelledError):
            exc = task.exception()
            if exc is not None:
                self._log(f"BLE bridge task failed: {exc}")

    def _notification_handler(self, outbound: asyncio.Queue[str]) -> Callable[[Any, bytearray], None]:
        buffer = bytearray()

        def handler(_: Any, data: bytearray) -> None:
            buffer.extend(data)
            while True:
                newline_index = buffer.find(b"\n")
                if newline_index < 0:
                    break
                raw_line = buffer[:newline_index].decode("utf-8", errors="replace").strip()
                del buffer[: newline_index + 1]
                if raw_line:
                    self._schedule_incoming_line(raw_line, outbound)

        return handler

    async def _write_line(self, client: Any, line: str) -> None:
        for chunk in self._frame_line(line):
            await client.write_gatt_char(RX_UUID, chunk, response=False)

    async def _writer_loop(self, client: Any, outbound: asyncio.Queue[str]) -> None:
        while True:
            line = await outbound.get()
            try:
                await self._write_line(client, line)
            finally:
                outbound.task_done()

    async def _event_loop(self, outbound: asyncio.Queue[str], subscriber_id: str) -> None:
        event_queue = await self.app.event_bus.subscribe(
            subscriber_id,
            EventFilter(types={EventType.APPROVAL_REQUEST, EventType.APPROVAL_RESPONSE, EventType.CODEX_STATUS, EventType.ERROR, EventType.STATUS_SNAPSHOT}),
        )
        try:
            while True:
                event = await event_queue.get()
                try:
                    if self._should_forward_event(event):
                        await outbound.put(json.dumps(event.to_dict(), ensure_ascii=False))
                finally:
                    event_queue.task_done()
        finally:
            await self.app.event_bus.unsubscribe(subscriber_id)

    async def _connection_watch_loop(self, client: Any) -> None:
        while getattr(client, "is_connected", False):
            await asyncio.sleep(0.25)

    async def _connect_once(self) -> bool:
        device = await self._discover_device()
        if device is None:
            self._log(f"BLE bridge: no {self.name_prefix} device found.")
            return False

        outbound: asyncio.Queue[str] = asyncio.Queue()
        subscriber_id = f"ble-{id(self)}-{id(device)}"
        factory = self.client_factory or BleakClient
        if factory is None:
            raise RuntimeError("bleak is required to use the BLE bridge.")

        self._log(f"BLE bridge: connecting to {getattr(device, 'name', None) or getattr(device, 'address', 'unknown')}.")
        async with factory(device) as client:
            await client.start_notify(TX_UUID, self._notification_handler(outbound))
            await outbound.put(self._server._bridge_connected_message())

            writer_task = asyncio.create_task(self._writer_loop(client, outbound))
            event_task = asyncio.create_task(self._event_loop(outbound, subscriber_id))
            watch_task = asyncio.create_task(self._connection_watch_loop(client))
            tasks = {writer_task, event_task, watch_task}

            try:
                done, _ = await asyncio.wait(tasks, return_when=asyncio.FIRST_COMPLETED)
                for task in done:
                    task.result()
            finally:
                for task in tasks:
                    task.cancel()
                for task in tasks:
                    with contextlib.suppress(asyncio.CancelledError, Exception):
                        await task
                for task in tuple(self._pending_inbound):
                    task.cancel()
                for task in tuple(self._pending_inbound):
                    with contextlib.suppress(asyncio.CancelledError, Exception):
                        await task

        return True

    async def run_forever(self) -> None:
        if not HAS_BLEAK and self.scanner is None and self.client_factory is None:
            raise RuntimeError("bleak is required to use the BLE bridge.")
        while True:
            try:
                await self._connect_once()
            except asyncio.CancelledError:
                raise
            except Exception as exc:
                self._log(f"BLE bridge error: {exc}")
            await asyncio.sleep(self.reconnect_delay_s)
