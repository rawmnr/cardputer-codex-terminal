# Cardputer Codex Terminal

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Firmware License: Apache 2.0](https://img.shields.io/badge/Firmware-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

A dedicated terminal for [OpenAI Codex](https://openai.com/codex) (or any compatible AI agent server), built for the [M5Stack Cardputer](https://shop.m5stack.com/).

> **Context:** This project provides the physical HID and display layer to drive a Codex "app-server" running on your host machine.

### 🧩 The Data Flow

```text
  [ User ] --(Input/Audio)--> [ Cardputer ] --(WS/BLE)--> [ Middleware (Host) ]
                                                                    |
                                                                    v
  [ Workspace ] <--(Files/Shell)-- [ Codex App-Server ] <----(Agent Protocol)
```

1.  **Cardputer**: Handheld interface. Handles LVGL UI, PDM audio capture, and Wi-Fi/BLE transport.
2.  **Middleware**: Orchestration layer (Python/uv). Handles transcription (`faster-whisper`), mDNS, and state sync.
3.  **App-Server**: The agentic "brain". Executes tasks, manages worktrees, and performs tool calls.

## 🚀 Key Features

*   **Push to Codex**: Hold `SPACE` for PTT voice prompts. Hardware-accelerated transcription.
*   **Agent Pager**: Multi-pane interface (Compose, Inbox, Detail) for long-form agent output.
*   **Telemetry Dashboard**: Real-time tracking of workspace status and token usage.
*   **MCP Bridge**: Exposes the Cardputer as a physical pager for any Model Context Protocol (MCP) agent.
*   **Local Preview**: Browser-based console (`:8787`) for debugging and visualization.

## 🏗 Architecture

*   **Middleware (Host)**: Python 3.11+ / `uv`. Manages WebSockets, audio transcription, and agent transport.
*   **Firmware (ESP32-S3)**: C++ / Arduino / LVGL. Handles reactive UI and real-time state sync.

## ⚡ Quick Start

### 1. Host Prep
Requires Python 3.11+ and [uv](https://github.com/astral.sh/uv).

```bash
cd middleware
uv sync
```

### 2. Device Prep
1.  Copy `firmware/config.example.ini` to `/cardputer-codex/config.ini` on a microSD.
2.  Add Wi-Fi/Bridge credentials to the config.
3.  Insert SD card into Cardputer.

### 3. Flash & Run
1.  Connect Cardputer via USB-C.
2.  Flash via PlatformIO:
    ```bash
    cd firmware
    pio run -t upload
    ```
3.  Start Middleware:
    ```bash
    cd middleware
    uv run cardputer-codex-middleware --serve --preview --real-codex
    ```

---

## ⌨️ Controls

| Key | Action |
| :--- | :--- |
| **`Ctrl + M`** | **App Menu** (Switch modes) |
| **`Fn + ; / .`** | **Up / Down** navigation |
| **`Enter` / `Del`** | **Accept** / **Reject** (Approvals) |
| **`Space` (Hold)** | **PTT** (Voice prompt) |
| **`/`** | **Command Palette** |

## 🔌 Connectivity

*   **Wi-Fi (Default)**: Low-latency WebSockets.
*   **BLE (Experimental)**: `--bridge-transport ble`.

## 📖 Resources

*   **[Dev Tutorial](docs/tutorial.md)** — First steps and core loops.
*   **[Architecture](docs/architecture.md)** — Deep dive into state sync.
*   **[Workflows](docs/workflows.md)** — Agentic Git patterns.
*   **[CLI Reference](docs/middleware.md)** — Advanced middleware flags.
*   **[Testing](docs/testing.md)** — HIL and contract validation.

## 🔍 Troubleshooting

*   **Connection**: Ensure same Wi-Fi. Check firewall for port `8765` and mDNS (`5353/UDP`).
*   **Transcription**: Ensure `uv sync` completed (downloads models).
*   **Logs**: Check `/cardputer-codex/log.txt` on the microSD.

## 📜 License

*   **Middleware**: [MIT](LICENSE)
*   **Firmware**: [Apache 2.0](LICENSE)
