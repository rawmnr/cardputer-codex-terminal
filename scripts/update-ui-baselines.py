from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MIDDLEWARE_SRC = ROOT / "middleware" / "src"
if str(MIDDLEWARE_SRC) not in sys.path:
    sys.path.insert(0, str(MIDDLEWARE_SRC))

from cardputer_codex_terminal.ui_regression import iter_scenario_paths, update_ui_baseline  # noqa: E402


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Update Cardputer UI simulator baselines.")
    parser.add_argument("--workspace-root", type=Path, default=ROOT)
    parser.add_argument("--binary", type=Path, default=None, help="Optional path to the simulator binary.")
    parser.add_argument("--scenario", action="append", default=[], help="Update only matching scenario name(s).")
    return parser


def main() -> int:
    args = build_parser().parse_args()
    scenario_paths = iter_scenario_paths(args.workspace_root)
    if args.scenario:
        wanted = set(args.scenario)
        scenario_paths = [path for path in scenario_paths if path.stem in wanted]

    for scenario_path in scenario_paths:
        baseline_path = update_ui_baseline(args.workspace_root, scenario_path, binary_path=args.binary)
        print(f"UPDATED {scenario_path.stem} -> {baseline_path.as_posix()}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
