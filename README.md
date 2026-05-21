# Cardputer Codex Terminal

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Firmware License: Apache 2.0](https://img.shields.io/badge/Firmware-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

A dedicated physical terminal and companion OS for [OpenAI Codex](https://openai.com/codex), built specifically for the [M5Stack Cardputer](https://shop.m5stack.com/).

This project bridges the gap between your cloud-based or local AI coding assistant and the physical world. It provides a tactile, glanceable, and voice-enabled interface to monitor, manage, and interact with Codex sessions without switching windows on your main workstation.

---

## 🚀 Key Features

| Feature | Description |
| :--- | :--- |
| **Codex Buddy** | A live dashboard showing your workspace, active branch, and real-time status. |
| **Push to Codex** | Hold **SPACE** to record a voice prompt. Hardware-accelerated transcription via `faster-whisper`. |
| **Codex Pager** | Manage long-running tasks with a three-pane interface (Compose, Inbox, Detail). |
| **Codex Usage** | Real-time telemetry tracking your primary and secondary rate limits. |
| **MCP Bridge** | Turn the Cardputer into a pocket pager for any Model Context Protocol (MCP) agent. |
| **Local Preview** | A browser-based central console (`http://127.0.0.1:8787`) for live debugging. |

---

## 🏗 Architecture

The system is split into two distinct, high-performance layers:

### 💻 Middleware (Host Machine)
The "brain" of the operation. A Python 3.11+ service managed by `uv` that handles:
*   **Connection Management**: Bridging the Cardputer via WebSockets.
*   **Heavy Lifting**: Audio transcription, mDNS discovery, and state orchestration.
*   **Codex Interface**: Acting as the standard `stdio` or `websocket` transport for the Codex app-server.

### 📟 Firmware (Cardputer Device)
The "interface" of the operation. A C++ firmware built on Arduino/ESP-IDF that handles:
*   **Reactive UI**: High-performance rendering using **LVGL**.
*   **Real-time Sync**: Uses an Epoch-based model to instantly update the screen when the host pushes state.
*   **Hardware I/O**: Managing the keyboard, PDM microphone, and display.

---

## 🛠 Getting Started

### 📋 Prerequisites

| Component | Requirement |
| :--- | :--- |
| **Hardware** | [M5Stack Cardputer](https://shop.m5stack.com/) (ESP32-S3) |
| **Storage** | MicroSD Card (for Wi-Fi/Bridge config and logging) |
| **Host Software** | [Python 3.11+](https://www.python.org/) & [`uv`](https://docs.astral.sh/uv/) |
| **Build Tools** | [PlatformIO](https://platformio.org/) |

### 1. Host Setup (Middleware)

1.  **Install Dependencies**:
    ```bash
    cd middleware
    uv sync
    ```

2.  **Start the Bridge**:
    ```bash
    # Standard local serve
    uv run cardputer-codex-middleware --serve --real-codex --host 0.0.0.0

    # Secure remote serve (Recommended)
    uv run cardputer-codex-middleware --serve --real-codex --bridge-token <your_secret>
    ```

### 2. Device Setup (Firmware)

1.  **Configure Wi-Fi**: Copy `firmware/config.example.ini` to `/cardputer-codex/config.ini` on your microSD card.
2.  **Flash Firmware**:
    ```bash
    cd firmware
    python -m platformio run --target upload
    ```

---

## ⌨️ Interface & Controls

### Global Shortcuts

| Key | Action |
| :--- | :--- |
| **`Ctrl + M`** | Open the **App Menu** to switch between modes. |
| **`Fn + ; / .`** | Navigate selection **Up / Down**. |
| **`Enter`** | Select, Approve, or Send. |
| **`Del`** | Back, Reject, or Cancel. |
| **`/`** | Open the **Command Palette**. |
| **`Space` (Hold)** | **Push-to-Talk** recording (in Push to Codex mode). |

### MCP Toolset
If running in MCP mode, Codex can interact with your device via:
*   `cardputer.notify(title, body)`: Send a notification banner.
*   `cardputer.ask(question, options)`: Prompt the user for a selection.
*   `cardputer.confirm(title)`: Require a physical button press for critical actions.

---

## 🔍 Troubleshooting

*   **Connection Issues**: Ensure the Cardputer and host are on the same Wi-Fi. Check firewall for port `8765` and mDNS (`5353/UDP`).
*   **Transcription Errors**: Ensure `uv sync` completed successfully so the transcription models could be downloaded.
*   **Logs**: Check `/cardputer-codex/log.txt` on the microSD card for detailed device-side errors.

---

## 📚 Documentation & Community

*   **[Architecture Guide](docs/architecture.md)** — Deep dive into the system design.
*   **[Middleware Docs](docs/middleware.md)** — API, CLI, and Developer Preview usage.
*   **[Testing Guide](docs/testing.md)** — Native firmware tests, middleware tests, contract fixtures, and HIL smoke checks.
*   **[Firmware Docs](docs/firmware.md)** — Hardware, UI, and Build instructions.
*   **[Security Guide](docs/security.md)** — Authentication and networking details.
*   **[Contributing](CONTRIBUTING.md)** — How to help improve the project.

---

## 📜 License

*   **Middleware**: [MIT](LICENSE)
*   **Firmware**: [Apache 2.0](LICENSE)
