Import("env")
import os

# Force GCC/G++ from MSYS2
mingw_bin = r"C:\tools\msys64\mingw64\bin"
env.Replace(
    CC=os.path.join(mingw_bin, "gcc.exe"),
    CXX=os.path.join(mingw_bin, "g++.exe"),
    LINK=os.path.join(mingw_bin, "g++.exe"),
    AR=os.path.join(mingw_bin, "ar.exe"),
    RANLIB=os.path.join(mingw_bin, "ranlib.exe")
)

# Add mingw to path for sub-processes
env.PrependENVPath("PATH", mingw_bin)
env.PrependENVPath("PATH", r"C:\tools\msys64\usr\bin")
