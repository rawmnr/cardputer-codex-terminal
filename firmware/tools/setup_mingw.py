Import("env")
import os

# Force GCC/G++ from MSYS2 when available.
mingw_bin = r"C:\tools\msys64\mingw64\bin"

if os.path.isdir(mingw_bin):
    env.Replace(
        CC=os.path.join(mingw_bin, "gcc.exe"),
        CXX=os.path.join(mingw_bin, "g++.exe"),
        LINK=os.path.join(mingw_bin, "g++.exe"),
        AR=os.path.join(mingw_bin, "ar.exe"),
        RANLIB=os.path.join(mingw_bin, "ranlib.exe"),
    )


    # Static link MinGW runtime to avoid DLL dependencies
    env.Append(LINKFLAGS=["-static-libgcc", "-static-libstdc++", "-static"])
    # Add mingw to path for sub-processes.
    env.PrependENVPath("PATH", mingw_bin)
    env.PrependENVPath("PATH", r"C:\tools\msys64\usr\bin")
