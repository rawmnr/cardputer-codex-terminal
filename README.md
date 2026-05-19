# Cardputer Codex Terminal

A dedicated physical terminal and companion OS for [OpenAI Codex](https://openai.com/codex), built specifically for the [M5Stack Cardputer](https://shop.m5stack.com/).

This project bridges the gap between your cloud-based or local AI coding assistant and the physical world. It provides a tactile, glanceable, and voice-enabled interface to monitor, manage, and interact with Codex sessions without switching windows on your main workstation.

## 🚀 Key Features

- **Codex Buddy** — A live dashboard showing your current workspace, active branch, running threads, and real-time status (Idle, Busy, Waiting for Approval).
- **Push to Codex** — Hold **SPACE** to record a voice prompt. The device streams audio to the host middleware, where `faster-whisper` transcribes it and forwards the text to Codex.
- **Codex Pager** — A three-pane interface (Compose, Inbox, Detail) to manage long-running AI tasks. Read live terminal output, approve pending tool calls, or issue a hardware `/interrupt` to halt a runaway agent.
- **Codex Usage** — High-resolution telemetry screens tracking your primary and secondary rate limits with human-readable reset countdowns.
- **MCP Bridge** — Turn the Cardputer into a pocket pager for any Model Context Protocol (MCP) compatible agent. The agent can send notifications, ask multiple-choice questions, or demand a physical confirmation gesture.
- **Local Preview** — A browser-based central console (`http://127.0.0.1:8787`) to monitor state, events, and a live snapshot of the Cardputer screen from your host.

## 🏗 Architecture

Unlike typical MicroPython setups, this project uses a high-performance **C++ Firmware** paired with a robust **Python Middleware** running on your host machine.

- **Firmware (`firmware/`)**: A highly responsive, state-driven UI built on Arduino/ESP-IDF using LVGL. It uses an Epoch-based Reactive Sync model—it doesn't poll; it instantly updates when the host pushes new state via WebSockets.
- **Middleware (`middleware/`)**: A Python 3.11+ service managed by `uv`. It acts as the brain, bridging the Cardputer to the Codex `app-server`. It handles heavy lifting like mDNS broadcast, audio transcription, and state orchestration.

---

## 🛠 Getting Started

### Hardware Prerequisites
- **M5Stack Cardputer** (ESP32-S3)
- **MicroSD Card** (Optional, for easy Wi-Fi/Bridge configuration)
- **Built-in PDM Microphone** (Required for Voice Transcription)

### 1. Middleware (Host) Setup
The middleware runs on your laptop or workstation and manages the connection to Codex.

1.  **Install Prerequisites**: Python 3.11+ and [`uv`](https://docs.astral.sh/uv/).
2.  **Sync Dependencies**:
    ```bash
    cd middleware
    uv sync
    ```
3.  **Start the Bridge**:
    ```bash
    uv run cardputer-codex-middleware --serve --real-codex --host 0.0.0.0
    ```
    *Note: If exposing beyond localhost, use `--bridge-token <secret>` for security.*

### 2. Firmware (Device) Setup
1.  **Install PlatformIO**: (CLI or VSCode Extension).
2.  **Connect & Flash**:
    ```bash
    cd firmware
    python -m platformio run --target upload
    ```
3.  **Expected Output**: `firmware/cardputer-codex-terminal.bin`. This binary is also compatible with M5 Launcher.

---

## ⚙️ Configuration

### SD Card Setup (Recommended)
Copy `firmware/config.example.ini` to `/cardputer-codex/config.ini` on your SD card.
```ini
wifi_ssid=YourNetworkName
wifi_password=YourWiFiPassword
middleware_host=192.168.1.50  # IP of your Windows/Mac/Linux host
middleware_port=8765
middleware_token=optional_secret
```

### Manual Config
Alternatively, edit `firmware/src/device_config.h` before flashing to hardcode credentials.

---

## ⌨️ Using the UI

The Cardputer uses a "Menu-First" navigation system.

### Global Shortcuts
- **`Ctrl + M`**: Open the **App Menu** to switch between Buddy, Pager, Voice, etc.
- **`Fn + ;` / `Fn + .`**: Move selection **Up / Down**.
- **`Enter`**: Select, Approve, or Send.
- **`Del`**: Back, Reject, or Cancel.
- **`/`**: Open the **Command Palette** from any screen.
- **`Space` (Hold)**: Push-to-talk recording (only in Push to Codex app).

### The Pager Flow
1.  **Inbox**: Scroll through recent threads. `Enter` to open.
2.  **Detail**: View live terminal output and event history.
3.  **Compose**: Press `/reply` or `/compose` to type a new task directly from the device.

---

## 🔌 Model Context Protocol (MCP)

The Cardputer Codex Terminal is a **two-way bridge**. Not only can you control Codex, but Codex can reach out and interact with you.

### Enable MCP Mode
Run the middleware with the `--mcp` flag:
```bash
uv run cardputer-codex-middleware --mcp --host 0.0.0.0
```

### Register with Agents (e.g. Claude Desktop)
Add the following to your `claude_desktop_config.json`:
```json
{
  "mcpServers": {
    "cardputer": {
      "command": "uv",
      "args": [
        "--project", "C:/path/to/cardputer-codex-terminal/middleware",
        "run", "cardputer-codex-middleware", "--mcp"
      ]
    }
  }
}
```

### Agent-to-Device Tools
- `notify(title, body)`: Flash a banner and chirp the speaker.
- `ask(question, options)`: Wait for a selection on the Cardputer keyboard.
- `confirm(title)`: Require a physical keypress before running destructive commands.

---

## 🔍 Troubleshooting

- **"Auto-discovering bridge..."**: Ensure the host and Cardputer are on the same Wi-Fi. Check firewall settings for port `8765` and mDNS (`5353/UDP`).
- **Voice Transcription hangs**: Ensure you ran `uv sync` to download the `faster-whisper` models. The first run may take a moment to load the model into memory.
- **"Message too big"**: Ensure you are using the latest firmware build.

## 📜 License
Middleware is licensed under **MIT**. Firmware is licensed under **Apache 2.0**.

## 📚 Documentation

- **[Architecture](docs/architecture.md)** — Deep dive into the firmware/middleware split.
- **[Middleware Guide](docs/middleware.md)** — CLI flags, message formats, and preview details.
- **[Firmware Guide](docs/firmware.md)** — UI implementation, keymaps, and build flags.
- **[Networking & Security](docs/networking.md)** — mDNS, `bridge_token`, and VPN setup.
- **[Roadmap](docs/roadmap.md)** — Future plans for BLE and native voice.

