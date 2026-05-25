# Firmware UI simulator and visual regression loop

## Build target

The host simulator uses the SDL3-backed native LVGL preview path in `firmware/simulator/sdl_main.cpp`.
Build it with:

```bash
cd firmware
python -m platformio run -e simulator
```

Set `SDL3_DIR` to the `x86_64-w64-mingw32` root from `SDL3-devel-3.4.8-mingw` if the package is not already on your PATH. For the copy used here, that is `C:\Users\rom1m\Downloads\SDL3-devel-3.4.8-mingw\SDL3-3.4.8\x86_64-w64-mingw32`.

The existing `preview` environment remains available, but `simulator` is the named target for the SDL3 host loop.

## Run a scenario

Scenarios live under `tests/ui/scenarios/` and are JSON fixtures consumed by `firmware/simulator/sdl_main.cpp`.
Each scenario can set an initial app and a step list of:

- `wait`
- `command`
- `text`
- `action`
- `key`
- `event` / `mock` / `network`

The simulator opens an SDL3 window for interactive runs, and exports a PNG after each scripted step for regression captures.

## Regression loop

Run all scenarios and compare against baselines:

```bash
./scripts/test-ui-sim.sh
```

Update approved baselines intentionally:

```bash
./scripts/update-ui-baselines.sh
```
On Windows, use the PowerShell wrappers:

```powershell
.\scripts\test-ui-sim.ps1
.\scripts\update-ui-baselines.ps1
```

Baseline PNGs live in `tests/ui/baselines/`. Failing runs write:

- `tests/ui/artifacts/*.actual.png`
- `tests/ui/artifacts/*.diff.png`
CI workflow is deferred to a follow-up; the local scripts are the supported regression loop for now.

## Keyboard mapping

The SDL3 preview/simulator accepts the same UI actions the firmware shell uses:

- arrows: menu navigation
- `Enter`: select / submit
- `Del` / `Esc`: back / dismiss
- printable keys: text input
- `Tab`: menu/focus advance

## Shared code

The simulator reuses the same firmware app shell, LVGL screens, and app logic as the Cardputer build. Only the host render/export loop is simulator-specific.
