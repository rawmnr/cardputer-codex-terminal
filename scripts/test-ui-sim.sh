#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" && pwd)"
uv run --directory "$SCRIPT_DIR/../middleware" python "$SCRIPT_DIR/test-ui-sim.py" "$@"
