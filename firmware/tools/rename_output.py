Import("env")

from pathlib import Path
import shutil


def rename_binary(source, target, env):
    build_dir = Path(env.subst("$BUILD_DIR"))
    original = build_dir / "firmware.bin"
    renamed = build_dir / "cardputer-codex-terminal.bin"
    if original.exists():
        shutil.copy2(original, renamed)


env.AddPostAction("$BUILD_DIR/firmware.bin", rename_binary)
