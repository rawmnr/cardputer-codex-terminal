import asyncio
import unittest
from cardputer_codex_terminal.bus import EventBus, EventFilter
from cardputer_codex_terminal.events import Event, EventType

class TestBus(unittest.TestCase):
    def test_publish_subscribe(self):
        async def scenario():
            bus = EventBus()
            queue = await bus.subscribe("sub1")
            
            event = Event(EventType.CODEX_STATUS, {"content": "test"})
            bus.publish(event)
            
            # We need to wait a bit because publish spawns a task
            received = await asyncio.wait_for(queue.get(), timeout=1.0)
            self.assertEqual(received.type, EventType.CODEX_STATUS)
            self.assertEqual(received.payload["content"], "test")
            return True

        asyncio.run(scenario())

    def test_multi_subscriber(self):
        async def scenario():
            bus = EventBus()
            q1 = await bus.subscribe("sub1")
            q2 = await bus.subscribe("sub2")
            
            event = Event(EventType.CODEX_STATUS, {"content": "shared"})
            bus.publish(event)
            
            r1 = await asyncio.wait_for(q1.get(), timeout=1.0)
            r2 = await asyncio.wait_for(q2.get(), timeout=1.0)
            
            self.assertEqual(r1.payload["content"], "shared")
            self.assertEqual(r2.payload["content"], "shared")
            return True

        asyncio.run(scenario())

    def test_filters(self):
        async def scenario():
            bus = EventBus()
            # Only status events
            q1 = await bus.subscribe("sub1", EventFilter(types={EventType.CODEX_STATUS}))
            # Only run_1 events
            q2 = await bus.subscribe("sub2", EventFilter(run_ids={"run_1"}))
            
            e1 = Event(EventType.CODEX_STATUS, {"run_id": "run_1"})
            e2 = Event(EventType.CODEX_DELTA, {"run_id": "run_1"})
            e3 = Event(EventType.CODEX_STATUS, {"run_id": "run_2"})
            
            bus.publish(e1)
            bus.publish(e2)
            bus.publish(e3)
            
            # q1 should get e1 and e3
            r1_a = await asyncio.wait_for(q1.get(), timeout=1.0)
            r1_b = await asyncio.wait_for(q1.get(), timeout=1.0)
            self.assertEqual(r1_a, e1)
            self.assertEqual(r1_b, e3)
            
            # q2 should get e1 and e2
            r2_a = await asyncio.wait_for(q2.get(), timeout=1.0)
            r2_b = await asyncio.wait_for(q2.get(), timeout=1.0)
            self.assertEqual(r2_a, e1)
            self.assertEqual(r2_b, e2)
            
            return True

        asyncio.run(scenario())

    def test_unsubscribe(self):
        async def scenario():
            bus = EventBus()
            q1 = await bus.subscribe("sub1")
            
            bus.publish(Event(EventType.CODEX_STATUS))
            await asyncio.wait_for(q1.get(), timeout=1.0)
            
            await bus.unsubscribe("sub1")
            bus.publish(Event(EventType.CODEX_STATUS))
            
            with self.assertRaises(asyncio.TimeoutError):
                await asyncio.wait_for(q1.get(), timeout=0.1)
                
            return True

        asyncio.run(scenario())

    def test_backpressure_drop_oldest(self):
        async def scenario():
            bus = EventBus()
            # Queue size 2
            q1 = await bus.subscribe("sub1", queue_size=2)
            
            bus.publish(Event(EventType.CODEX_STATUS, {"id": 1}))
            bus.publish(Event(EventType.CODEX_STATUS, {"id": 2}))
            bus.publish(Event(EventType.CODEX_STATUS, {"id": 3})) # Should drop id 1
            
            # Give some time for tasks to run
            await asyncio.sleep(0.1)
            
            r1 = await q1.get()
            r2 = await q1.get()
            
            self.assertEqual(r1.payload["id"], 2)
            self.assertEqual(r2.payload["id"], 3)
            
            return True

        asyncio.run(scenario())

if __name__ == "__main__":
    unittest.main()
