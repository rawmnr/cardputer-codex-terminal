from __future__ import annotations

import base64
import json
import subprocess
import tempfile
from pathlib import Path
from typing import Any


def _screen_label_to_app_id(screen: str) -> int | None:
    if screen == "buddy":
        return 0
    if screen == "push":
        return 1
    if screen == "pager":
        return 2
    if screen == "usage":
        return 3
    if screen == "bridge":
        return 4
    if screen == "settings":
        return 5
    return None


def _collect_frame_paths(output_prefix: Path) -> list[Path]:
    frame_paths = list(output_prefix.parent.glob(f"{output_prefix.name}_*.png"))
    if not frame_paths and output_prefix.exists():
        frame_paths = [output_prefix]

    def sort_key(path: Path) -> tuple[int, str, str]:
        stem = path.stem
        prefix = f"{output_prefix.name}_"
        if stem.startswith(prefix):
            remainder = stem[len(prefix) :]
            step_text, _, label = remainder.partition("_")
            if step_text.isdigit():
                return (int(step_text), label, path.name)
        return (0, path.name, path.name)

    frame_paths.sort(key=sort_key)
    return frame_paths


def _frame_label(output_prefix: Path, frame_path: Path) -> str:
    stem = frame_path.stem
    prefix = f"{output_prefix.name}_"
    if stem.startswith(prefix):
        remainder = stem[len(prefix) :]
        step_text, sep, label = remainder.partition("_")
        if sep and step_text.isdigit() and label:
            return label
    if stem == output_prefix.name:
        return "initial"
    return stem


def run_lvgl_preview(
    workspace_root: Path,
    screen: str = "buddy",
    fixture: str | dict[str, Any] | None = None,
    actions: list[str] | None = None,
    binary_path: Path | None = None,
) -> dict[str, Any]:
    """
    Runs the native LVGL preview binary and returns the rendered frames as base64.
    """
    if binary_path is None:
        candidates = [
            workspace_root / "firmware" / ".pio" / "build" / "preview" / "program.exe",
            workspace_root / "firmware" / "preview.exe",
            workspace_root / "firmware" / ".pio" / "build" / "preview" / "program",
            workspace_root / "firmware" / "preview",
        ]
        for candidate in candidates:
            if candidate.exists():
                binary_path = candidate
                break

    if binary_path is None or not binary_path.exists():
        raise RuntimeError(
            "LVGL preview binary not found. Please build the 'preview' environment in the firmware directory."
        )

    fixture_data: dict[str, Any] = {}
    if isinstance(fixture, dict):
        fixture_data = fixture
    elif isinstance(fixture, str):
        fixture_file = workspace_root / "firmware" / "preview" / "fixtures" / f"{fixture}.json"
        if fixture_file.exists():
            fixture_data = json.loads(fixture_file.read_text(encoding="utf-8"))

    app_id = _screen_label_to_app_id(screen)
    if app_id is not None:
        fixture_data["active_app"] = app_id

    if actions:
        fixture_data["actions"] = actions

    with tempfile.NamedTemporaryFile(mode="w", suffix=".json", delete=False, encoding="utf-8") as tmp_fixture:
        json.dump(fixture_data, tmp_fixture)
        tmp_fixture_path = Path(tmp_fixture.name)

    try:
        with tempfile.TemporaryDirectory() as tmp_output_dir_name:
            output_prefix = Path(tmp_output_dir_name) / "preview"

            result = subprocess.run(
                [str(binary_path), str(tmp_fixture_path), str(output_prefix)],
                capture_output=True,
                text=True,
                cwd=str(workspace_root / "firmware"),
            )

            frame_paths = _collect_frame_paths(output_prefix)
            if result.returncode != 0 and not frame_paths:
                raise RuntimeError(f"Preview binary failed: {result.stderr or result.stdout}")
            if not frame_paths:
                raise RuntimeError("Preview binary did not generate an output image.")

            frames: list[dict[str, Any]] = []
            for index, frame_path in enumerate(frame_paths):
                frame_bytes = frame_path.read_bytes()
                frames.append(
                    {
                        "index": index,
                        "label": _frame_label(output_prefix, frame_path),
                        "filename": frame_path.name,
                        "width": 240,
                        "height": 135,
                        "image_b64": base64.b64encode(frame_bytes).decode("utf-8"),
                    }
                )

            return {
                "image_b64": frames[-1]["image_b64"],
                "frame_count": len(frames),
                "frames": frames,
                "width": 240,
                "height": 135,
                "fixture": fixture_data,
                "actions": list(actions or []),
                "stdout": result.stdout,
                "stderr": result.stderr,
                "returncode": result.returncode,
            }

    finally:
        if tmp_fixture_path.exists():
            tmp_fixture_path.unlink()
