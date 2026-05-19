from __future__ import annotations

import asyncio
from dataclasses import dataclass, field
from typing import Any, Callable, Set

from .events import Event, EventType


@dataclass(slots=True, frozen=True)
class EventFilter:
    types: Set[EventType] | None = None
    run_ids: Set[str] | None = None
    session_ids: Set[str] | None = None
    min_verbosity: int = 0  # 0: normal, 1: verbose, 2: debug

    def matches(self, event: Event, metadata: dict[str, Any] | None = None) -> bool:
        if self.types is not None and event.type not in self.types:
            return False

        # Check metadata then payload for run_id / session_id
        if self.run_ids is not None:
            run_id = (metadata or {}).get("run_id") or event.payload.get("run_id")
            if run_id not in self.run_ids:
                return False

        if self.session_ids is not None:
            session_id = (metadata or {}).get("session_id") or event.payload.get("session_id")
            if session_id not in self.session_ids:
                return False

        verbosity = (metadata or {}).get("verbosity") or event.payload.get("verbosity", 0)
        if verbosity < self.min_verbosity:
            return False

        return True


class Subscriber:
    def __init__(self, subscriber_id: str, filters: EventFilter, queue_size: int = 100) -> None:
        self.subscriber_id = subscriber_id
        self.filters = filters
        self.queue: asyncio.Queue[Event] = asyncio.Queue(maxsize=queue_size)

    async def put(self, event: Event, metadata: dict[str, Any] | None = None) -> None:
        if self.filters.matches(event, metadata):
            try:
                self.queue.put_nowait(event)
            except asyncio.QueueFull:
                # Basic backpressure: drop oldest if full to keep stream live
                # Alternatively, we could drop the new one.
                # For a real-time terminal, dropping oldest is often better.
                try:
                    self.queue.get_nowait()
                    self.queue.put_nowait(event)
                except (asyncio.QueueEmpty, asyncio.QueueFull):
                    pass


class EventBus:
    def __init__(self) -> None:
        self._subscribers: dict[str, Subscriber] = {}
        self._lock = asyncio.Lock()

    async def subscribe(
        self, 
        subscriber_id: str, 
        filters: EventFilter | None = None,
        queue_size: int = 100
    ) -> asyncio.Queue[Event]:
        async with self._lock:
            subscriber = Subscriber(
                subscriber_id, 
                filters or EventFilter(),
                queue_size=queue_size
            )
            self._subscribers[subscriber_id] = subscriber
            return subscriber.queue

    async def unsubscribe(self, subscriber_id: str) -> None:
        async with self._lock:
            if subscriber_id in self._subscribers:
                del self._subscribers[subscriber_id]

    def publish(self, event: Event, metadata: dict[str, Any] | None = None) -> None:
        # We don't want publish to be async to avoid blocking the main logic
        # Subscribers' queues are async-safe
        for subscriber in list(self._subscribers.values()):
            # We use a task to avoid blocking the caller if put() were ever async
            # but Subscriber.put() is sync-wrapped with put_nowait
            asyncio.create_task(subscriber.put(event, metadata))

    def publish_many(self, events: list[Event], metadata: dict[str, Any] | None = None) -> None:
        for event in events:
            self.publish(event, metadata)

