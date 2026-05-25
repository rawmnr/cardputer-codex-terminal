Import("env")
import os
from pathlib import Path

# Prefer a repo-local toolchain extracted from the bundled mingw.7z archive,
# then fall back to a system-wide MSYS2 install if present.
repo_root = Path(env['PROJECT_DIR']).parent
candidates = [
    repo_root / ".toolchains" / "msys64" / "mingw64" / "bin",
    Path(r"C:\tools\msys64\mingw64\bin"),
]

for mingw_bin_path in candidates:
    if os.path.isdir(mingw_bin_path):
        mingw_bin = str(mingw_bin_path)
        env.Replace(
            CC=os.path.join(mingw_bin, "gcc.exe"),
            CXX=os.path.join(mingw_bin, "g++.exe"),
            LINK=os.path.join(mingw_bin, "g++.exe"),
            AR=os.path.join(mingw_bin, "ar.exe"),
            RANLIB=os.path.join(mingw_bin, "ranlib.exe"),
        )

        # Static link MinGW runtime to avoid DLL dependencies.
        env.Append(LINKFLAGS=["-static-libgcc", "-static-libstdc++", "-static"])
        env.PrependENVPath("PATH", mingw_bin)
        env.PrependENVPath("PATH", os.path.join(str(mingw_bin_path.parent), "usr", "bin"))
        break
