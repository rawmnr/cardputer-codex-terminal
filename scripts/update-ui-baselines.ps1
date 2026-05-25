$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
uv run --directory "$ScriptDir\..\middleware" python "$ScriptDir\update-ui-baselines.py" @args
