from __future__ import annotations

from dataclasses import dataclass, field
import base64
from typing import Protocol


class VoiceTranscriber(Protocol):
    async def transcribe(self, pcm_bytes: bytes, sample_rate_hz: int) -> str:
        raise NotImplementedError


@dataclass(slots=True)
class MockVoiceTranscriber:
    async def transcribe(self, pcm_bytes: bytes, sample_rate_hz: int) -> str:
        sample_count = len(pcm_bytes) // 2
        duration_ms = int((sample_count / sample_rate_hz) * 1000) if sample_rate_hz > 0 else 0
        return f"Voice prompt captured ({sample_count} samples, {duration_ms} ms at {sample_rate_hz} Hz)."


@dataclass(slots=True)
class VoicePromptBuffer:
    pcm_chunks: list[bytes] = field(default_factory=list)
    sample_rate_hz: int = 16000
    chunk_count: int = 0

    def append_chunk(self, pcm_b64: str) -> int:
        pcm_bytes = base64.b64decode(pcm_b64)
        self.pcm_chunks.append(pcm_bytes)
        self.chunk_count += 1
        return len(pcm_bytes) // 2

    def reset(self) -> None:
        self.pcm_chunks.clear()
        self.chunk_count = 0

    def has_audio(self) -> bool:
        return len(self.pcm_chunks) > 0

    def sample_count(self) -> int:
        return sum(len(chunk) for chunk in self.pcm_chunks) // 2

    def byte_count(self) -> int:
        return sum(len(chunk) for chunk in self.pcm_chunks)

    async def transcribe(self, transcriber: VoiceTranscriber) -> str:
        pcm_bytes = b"".join(self.pcm_chunks)
        self.reset()
        return await transcriber.transcribe(pcm_bytes, self.sample_rate_hz)
