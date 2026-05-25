from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MIDDLEWARE_SRC = ROOT / "middleware" / "src"
if str(MIDDLEWARE_SRC) not in sys.path:
    sys.path.insert(0, str(MIDDLEWARE_SRC))

from cardputer_codex_terminal.ui_regression import (  # noqa: E402
    artifact_dir,
    run_all_ui_scenarios,
)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Run the Cardputer UI simulator regression loop.")
    parser.add_argument("--workspace-root", type=Path, default=ROOT)
    parser.add_argument("--binary", type=Path, default=None, help="Optional path to the simulator binary.")
    parser.add_argument("--scenario", action="append", default=[], help="Run only matching scenario name(s).")
    return parser


def main() -> int:
    args = build_parser().parse_args()
    results = run_all_ui_scenarios(args.workspace_root, binary_path=args.binary)
    if args.scenario:
        wanted = set(args.scenario)
        results = [result for result in results if result.name in wanted]

    artifact_dir(args.workspace_root).mkdir(parents=True, exist_ok=True)

    failures = []
    for result in results:
        if result.passed:
            print(f"PASS {result.name}")
        else:
            failures.append(result)
            print(f"FAIL {result.name}")
            print(f"  expected: {result.baseline_path.as_posix()}")
            print(f"  actual:   {result.actual_path.as_posix()}")
            print(f"  diff:     {result.diff_path.as_posix()}")
            print(f"  frames:   {', '.join(result.frame_labels) if result.frame_labels else '-'}")
            print(f"  note:     {result.message}")

    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
