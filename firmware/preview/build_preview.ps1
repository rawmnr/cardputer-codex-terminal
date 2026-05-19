$MSVC_PATH = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64"
$CL = "$MSVC_PATH\cl.exe"

$INCLUDES = @(
    "/I", "preview",
    "/I", "src",
    "/I", "src/ui",
    "/I", "include",
    "/I", "lib/lvgl/src",
    "/I", "lib/ArduinoJson/src"
)

$SOURCES = @(
    "src/ui/lvgl_port_preview.cpp",
    "src/ui/lvgl_screen.cpp",
    "src/ui/buddy_screen.cpp",
    "src/ui/bridge_screen.cpp",
    "src/ui/pager_screen.cpp",
    "src/ui/push_screen.cpp",
    "src/ui/settings_screen.cpp",
    "src/ui/usage_screen.cpp",
    "src/ui/ptt_widget.cpp",
    "src/ui/modal.cpp",
    "preview/preview_main.cpp"
)

$FLAGS = @(
    "/std:c++17",
    "/D", "USE_LVGL_UI=1",
    "/D", "ARDUINOJSON_ENABLE_STD_STREAM=1",
    "/EHsc",
    "/O2",
    "/Fe:preview.exe"
)

Write-Host "Building LVGL Preview with MSVC..."
& $CL $FLAGS $INCLUDES $SOURCES

if ($LASTEXITCODE -eq 0) {
    Write-Host "Success! preview.exe created."
} else {
    Write-Host "Build failed." -ForegroundColor Red
}
