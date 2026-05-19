# Cardputer Codex Firmware

The embedded firmware for the M5Stack Cardputer ADV, designed to transform the device into a physical terminal for the OpenAI Codex ecosystem.

This firmware manages the hardware interface, UI rendering via LVGL, Wi-Fi connectivity, and the WebSocket communication bridge to the Windows middleware.

---

## 🚀 Overview

The firmware provides a seamless, interactive terminal experience on the Cardputer's 240x135 display. Key features include:

*   **Interactive UI**: A sophisticated interface built with LVGL for navigating apps, browsing sessions, and managing settings.
*   **Codex Integration**: Real-time streaming of Codex responses and event handling via the WebSocket bridge.
*   **Push-to-Talk (PTT)**: Hardware-triggered voice input pipeline.
*   **Robust Connectivity**: Managed Wi-Fi and secure bridge authentication.
*   **Persistence**: Configuration and logging stored on the microSD card.

---

## 🛠 Build Instructions

The firmware is built using [PlatformIO](https://platformio.org/).

### Prerequisites

*   [Python 3.x](https://www.python.org/)
*   [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation.html)

### Compilation

To compile the firmware for the Cardputer:

```bash
cd firmware
python -m platformio run
```

### Build Output

The build process includes a post-build script that renames the output for compatibility with the M5 Launcher. The resulting binary is located at:

`firmware/cardputer-codex-terminal.bin`

---

## 📲 Deployment

1.  **Flashing**: Use the [M5 Launcher](https://github.com/m5stack/M5Launcher) or a standard ESP32 flashing tool to upload the `.bin` file to your Cardputer.
2.  **SD Card Setup**: Ensure a microSD card is inserted. The firmware will automatically create a `/cardputer-codex/` directory on the first boot.

---

## ⚙️ Configuration

Connectivity and middleware settings are managed via a simple key-value configuration file stored on the microSD card.

**Path**: `/cardputer-codex/config.ini`

### Example Configuration

You can use the provided template as a starting point:

```ini
# Wi-Fi Settings
wifi_ssid=YourNetworkName
wifi_password=YourWiFiPassword

# Middleware Bridge Settings
# Use the IP address of your Windows host
middleware_host=192.168.1.50
middleware_port=8765
middleware_path=/
middleware_token=your_shared_secret_here
```

*Note: `127.0.0.1` refers to the Cardputer itself. Always use the host machine's LAN IP or hostname to connect to the middleware.*

---

## 🔍 Logging & Diagnostics

For debugging connectivity or startup issues, the firmware appends a plain text log to the microSD card.

**Path**: `/cardputer-codex/log.txt`

This log is persistent across firmware updates and is the primary tool for diagnosing Wi-Fi, SD card, or bridge connection failures.
