from __future__ import annotations

import base64
import json
import subprocess
import tempfile
from pathlib import Path
from typing import Any


def run_lvgl_preview(
    workspace_root: Path,
    screen: str = "buddy",
    fixture: str | dict[str, Any] | None = None,
    actions: list[str] | None = None,
    binary_path: Path | None = None,
) -> dict[str, Any]:
    """
    Runs the native LVGL preview binary and returns the resulting image as base64.
    """
    if binary_path is None:
        # Try common locations
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

    # Prepare the fixture
    fixture_data: dict[str, Any] = {}
    if isinstance(fixture, dict):
        fixture_data = fixture
    elif isinstance(fixture, str):
        fixture_file = workspace_root / "firmware" / "preview" / "fixtures" / f"{fixture}.json"
        if fixture_file.exists():
            fixture_data = json.loads(fixture_file.read_text(encoding="utf-8"))
        else:
            # Fallback to empty fixture if named one is missing
            fixture_data = {}
    
    # Override screen if provided
    if screen == "buddy":
        fixture_data["active_app"] = 0
    elif screen == "push":
        fixture_data["active_app"] = 1
    elif screen == "pager":
        fixture_data["active_app"] = 2
    elif screen == "usage":
        fixture_data["active_app"] = 3
    elif screen == "bridge":
        fixture_data["active_app"] = 4
    elif screen == "settings":
        fixture_data["active_app"] = 5

    if actions:
        fixture_data["actions"] = actions

    with tempfile.NamedTemporaryFile(mode="w", suffix=".json", delete=False, encoding="utf-8") as tmp_fixture:
        json.dump(fixture_data, tmp_fixture)
        tmp_fixture_path = Path(tmp_fixture.name)

    try:
        with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as tmp_output:
            tmp_output_path = Path(tmp_output.name)

        # Run the preview binary
        # Command line: preview.exe <fixture_path> <output_path>
        result = subprocess.run(
            [str(binary_path), str(tmp_fixture_path), str(tmp_output_path)],
            capture_output=True,
            text=True,
            cwd=str(workspace_root / "firmware"),
        )

        if result.returncode != 0:
            raise RuntimeError(f"Preview binary failed: {result.stderr or result.stdout}")

        if not tmp_output_path.exists():
            raise RuntimeError("Preview binary did not generate an output image.")

        # Read the image and encode as base64
        image_bytes = tmp_output_path.read_bytes()
        image_b64 = base64.b64encode(image_bytes).decode("utf-8")

        return {
            "image_b64": image_b64,
            "width": 240,
            "height": 135,
            "fixture": fixture_data,
            "stdout": result.stdout,
            "stderr": result.stderr,
        }

    finally:
        # Cleanup temporary files
        if tmp_fixture_path.exists():
            tmp_fixture_path.unlink()
        if 'tmp_output_path' in locals() and tmp_output_path.exists():
            tmp_output_path.unlink()
