from __future__ import annotations

from pathlib import Path
import unittest

from cardputer_codex_terminal.ui_regression import run_all_ui_scenarios


class UiRegressionTests(unittest.TestCase):
    def test_ui_scenarios_match_baselines(self) -> None:
        workspace_root = Path(__file__).resolve().parents[2]
        results = run_all_ui_scenarios(workspace_root)
        failures = [result for result in results if not result.passed]

        if failures:
            lines = []
            for result in failures:
                lines.append(f"{result.name}: {result.message}")
                lines.append(f"  expected: {result.baseline_path.as_posix()}")
                lines.append(f"  actual:   {result.actual_path.as_posix()}")
                lines.append(f"  diff:     {result.diff_path.as_posix()}")
                lines.append(f"  frames:   {', '.join(result.frame_labels) if result.frame_labels else '-'}")
            self.fail("\n".join(lines))


if __name__ == "__main__":
    unittest.main()
