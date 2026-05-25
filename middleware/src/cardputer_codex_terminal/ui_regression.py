from __future__ import annotations

import base64
import io
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from PIL import Image, ImageChops

from .lvgl_preview import run_lvgl_preview


@dataclass(frozen=True, slots=True)
class UiScenarioResult:
    name: str
    scenario_path: Path
    baseline_path: Path
    actual_path: Path
    diff_path: Path
    passed: bool
    message: str
    frame_count: int
    frame_labels: tuple[str, ...]
    returncode: int


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def ui_root(workspace_root: Path | None = None) -> Path:
    return (workspace_root or repo_root()) / "tests" / "ui"


def scenario_dir(workspace_root: Path | None = None) -> Path:
    return ui_root(workspace_root) / "scenarios"


def baseline_dir(workspace_root: Path | None = None) -> Path:
    return ui_root(workspace_root) / "baselines"


def artifact_dir(workspace_root: Path | None = None) -> Path:
    return ui_root(workspace_root) / "artifacts"


def load_scenario(scenario_path: Path) -> dict[str, Any]:
    return json.loads(scenario_path.read_text(encoding="utf-8"))


def iter_scenario_paths(workspace_root: Path | None = None) -> list[Path]:
    return sorted(scenario_dir(workspace_root).glob("*.json"))


def scenario_name(scenario_path: Path) -> str:
    return scenario_path.stem


def baseline_path_for(scenario_path: Path, workspace_root: Path | None = None) -> Path:
    return baseline_dir(workspace_root) / f"{scenario_name(scenario_path)}.png"


def actual_path_for(scenario_path: Path, workspace_root: Path | None = None) -> Path:
    return artifact_dir(workspace_root) / f"{scenario_name(scenario_path)}.actual.png"


def diff_path_for(scenario_path: Path, workspace_root: Path | None = None) -> Path:
    return artifact_dir(workspace_root) / f"{scenario_name(scenario_path)}.diff.png"


def _decode_png(image_b64: str) -> bytes:
    return base64.b64decode(image_b64.encode("utf-8"))


def _load_image(data: bytes) -> Image.Image:
    return Image.open(io.BytesIO(data)).convert("RGBA")


def _save_png(path: Path, image: Image.Image) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path, format="PNG")


def _diff_images(expected: Image.Image, actual: Image.Image) -> Image.Image:
    diff = ImageChops.difference(expected, actual)
    if diff.getbbox() is None:
        return diff
    return diff


def compare_png_bytes(expected_png: bytes, actual_png: bytes) -> tuple[bool, str, bytes | None]:
    expected = _load_image(expected_png)
    actual = _load_image(actual_png)

    if expected.size != actual.size:
        diff = _diff_images(expected.resize(actual.size), actual) if expected.size != actual.size else _diff_images(expected, actual)
        buffer = io.BytesIO()
        diff.save(buffer, format="PNG")
        return False, f"size mismatch: expected {expected.size}, actual {actual.size}", buffer.getvalue()

    diff = _diff_images(expected, actual)
    if diff.getbbox() is None:
        return True, "", None

    buffer = io.BytesIO()
    diff.save(buffer, format="PNG")
    return False, "pixel mismatch", buffer.getvalue()


def run_ui_scenario(workspace_root: Path, scenario_path: Path, binary_path: Path | None = None) -> UiScenarioResult:
    scenario = load_scenario(scenario_path)
    result = run_lvgl_preview(
        workspace_root=workspace_root,
        screen=scenario.get("screen", "buddy"),
        fixture=scenario,
        actions=scenario.get("actions"),
        binary_path=binary_path,
    )

    final_png = _decode_png(result["image_b64"])
    baseline_path = baseline_path_for(scenario_path, workspace_root)
    actual_path = actual_path_for(scenario_path, workspace_root)
    diff_path = diff_path_for(scenario_path, workspace_root)

    actual_path.parent.mkdir(parents=True, exist_ok=True)
    actual_path.write_bytes(final_png)

    frame_labels = tuple(str(frame["label"]) for frame in result["frames"])

    if int(result["returncode"]) != 0:
        message = f"simulator exited with code {result['returncode']}"
        return UiScenarioResult(
            name=scenario_name(scenario_path),
            scenario_path=scenario_path,
            baseline_path=baseline_path,
            actual_path=actual_path,
            diff_path=diff_path,
            passed=False,
            message=message,
            frame_count=int(result["frame_count"]),
            frame_labels=frame_labels,
            returncode=int(result["returncode"]),
        )

    if not baseline_path.exists():
        message = f"missing baseline: {baseline_path.as_posix()}"
        return UiScenarioResult(
            name=scenario_name(scenario_path),
            scenario_path=scenario_path,
            baseline_path=baseline_path,
            actual_path=actual_path,
            diff_path=diff_path,
            passed=False,
            message=message,
            frame_count=int(result["frame_count"]),
            frame_labels=frame_labels,
            returncode=int(result["returncode"]),
        )

    passed, message, diff_png = compare_png_bytes(baseline_path.read_bytes(), final_png)
    if passed:
        if actual_path.exists():
            actual_path.unlink()
        if diff_path.exists():
            diff_path.unlink()
    else:
        if diff_png is not None:
            diff_path.write_bytes(diff_png)

    return UiScenarioResult(
        name=scenario_name(scenario_path),
        scenario_path=scenario_path,
        baseline_path=baseline_path,
        actual_path=actual_path,
        diff_path=diff_path,
        passed=passed,
        message=message,
        frame_count=int(result["frame_count"]),
        frame_labels=frame_labels,
        returncode=int(result["returncode"]),
    )


def update_ui_baseline(workspace_root: Path, scenario_path: Path, binary_path: Path | None = None) -> Path:
    scenario = load_scenario(scenario_path)
    result = run_lvgl_preview(
        workspace_root=workspace_root,
        screen=scenario.get("screen", "buddy"),
        fixture=scenario,
        actions=scenario.get("actions"),
        binary_path=binary_path,
    )
    if int(result["returncode"]) != 0:
        raise RuntimeError(f"simulator exited with code {result['returncode']} while updating {scenario_path.stem}")
    baseline_path = baseline_path_for(scenario_path, workspace_root)
    baseline_path.parent.mkdir(parents=True, exist_ok=True)
    baseline_path.write_bytes(_decode_png(result["image_b64"]))
    return baseline_path


def run_all_ui_scenarios(workspace_root: Path, binary_path: Path | None = None) -> list[UiScenarioResult]:
    results: list[UiScenarioResult] = []
    for scenario_path in iter_scenario_paths(workspace_root):
        results.append(run_ui_scenario(workspace_root, scenario_path, binary_path=binary_path))
    return results
