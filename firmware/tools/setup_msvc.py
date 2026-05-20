Import("env")
import os

if os.name == "nt":
    msvc_bin = r"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64"
    # Force MSVC flags for native platform on Windows
    env.Replace(
        CC=os.path.join(msvc_bin, "cl.exe"),
        CXX=os.path.join(msvc_bin, "cl.exe"),
        LINK=os.path.join(msvc_bin, "link.exe"),
        AR=os.path.join(msvc_bin, "lib.exe"),
        RANLIB="echo"
    )
    env.Append(
        CCFLAGS=["/nologo", "/W3", "/EHsc", "/O2"],
        CPPDEFINES=["WIN32", "_CONSOLE"],
        CPPPATH=[
            r"C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\ucrt",
            r"C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um",
            r"C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\shared",
            r"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\MSVC\14.51.36231\include"
        ],
        LIBPATH=[
            r"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\MSVC\14.51.36231\lib\x64",
            r"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64",
            r"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64"
        ]
    )
