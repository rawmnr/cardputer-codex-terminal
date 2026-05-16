Import("env")

from pathlib import Path
import shutil


def rename_binary(source, target, env):
    build_dir = Path(env.subst("$BUILD_DIR"))
    project_dir = Path(env.subst("$PROJECT_DIR"))
    original = build_dir / "firmware.bin"
    renamed = build_dir / "cardputer-codex-terminal.bin"
    project_copy = project_dir / "cardputer-codex-terminal.bin"
    if original.exists():
        shutil.copy2(original, renamed)
        shutil.copy2(original, project_copy)


env.AddPostAction("$BUILD_DIR/firmware.bin", rename_binary)
