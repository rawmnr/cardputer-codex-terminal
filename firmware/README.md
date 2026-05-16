# Firmware

This folder is reserved for the Cardputer firmware source tree.

## Current Scaffold

The first firmware pass uses PlatformIO with a small Arduino-based shell that can already build into a flashable `.bin`.

## Build Target

The firmware must compile into a single flashable `.bin` file that can be launched with M5 Launcher on the Cardputer.

## Expected Output

```text
cardputer-codex-terminal.bin
```

## Working Assumptions

- The source layout should be chosen so the build system emits a M5 Launcher-compatible binary.
- The binary is the primary deliverable, not a desktop executable or loose script bundle.
- Runtime assets should be embedded or packaged in a way that still results in a single firmware binary.
- The first code path focuses on the shell, state model, and app switching so we can wire the real Cardputer backends next.
