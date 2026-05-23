@echo off
set PATH=%PATH%;C:\tools\msys64\mingw64\bin
echo Running preview with MinGW in PATH...
.pio\build\preview\program.exe preview\fixtures\buddy_idle.json test_output
echo Exit code: %ERRORLEVEL%
dir test_output*.png 2>nul
