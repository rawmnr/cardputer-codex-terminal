@echo off
echo Running preview...
.pio\build\preview\program.exe preview\fixtures\buddy_idle.json test_output
echo Exit code: %ERRORLEVEL%
dir test_output*.png 2>nul
