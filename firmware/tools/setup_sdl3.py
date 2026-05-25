Import("env")

import os
import shutil
from pathlib import Path

repo_root = Path(env["PROJECT_DIR"]).parent
candidate_roots = []

sdl3_env = os.environ.get("SDL3_DIR", "").strip()
if sdl3_env:
    candidate_roots.append(Path(sdl3_env))

candidate_roots.extend(
    [
        Path(r"C:\Users\rom1m\Downloads\SDL3-devel-3.4.8-mingw\SDL3-3.4.8\x86_64-w64-mingw32"),
        Path(r"C:\Users\rom1m\Downloads\SDL3-3.4.8-win32-x64"),
        repo_root / ".toolchains" / "SDL3" / "x86_64-w64-mingw32",
    ]
)

sdl3_root = None
for candidate in candidate_roots:
    include_dir = candidate / "include"
    lib_dir = candidate / "lib"
    dll_path = candidate / "bin" / "SDL3.dll"
    if include_dir.is_dir() and lib_dir.is_dir() and dll_path.exists():
        sdl3_root = candidate
        break

if sdl3_root is None:
    raise SystemExit(
        "SDL3 development package not found. Set SDL3_DIR to the x86_64-w64-mingw32 root from SDL3-devel-3.4.8-mingw."
    )

include_dir = (sdl3_root / "include").as_posix()
lib_dir = sdl3_root / "lib"
dll_path = sdl3_root / "bin" / "SDL3.dll"
toolchain_lib_dir = repo_root / ".toolchains" / "msys64" / "mingw64" / "x86_64-w64-mingw32" / "lib"

toolchain_lib_dir.mkdir(parents=True, exist_ok=True)
shutil.copy2(lib_dir / "libSDL3.dll.a", toolchain_lib_dir / "libSDL3.dll.a")
shutil.copy2(lib_dir / "libSDL3.dll.a", toolchain_lib_dir / "libSDL3.a")

env.Append(CPPPATH=[include_dir])
env.Append(LIBPATH=[toolchain_lib_dir.as_posix()])
env.Append(LIBS=["SDL3"])


def copy_sdl3_dll(source, target, env):
    build_dir = Path(env.subst("$BUILD_DIR"))
    destination = build_dir / "SDL3.dll"
    shutil.copy2(dll_path, destination)


env.AddPostAction("$PROGPATH", copy_sdl3_dll)
