# Cardputer Codex Terminal

A dedicated physical terminal and companion OS for [OpenAI Codex](https://openai.com/codex), built specifically for the [M5Stack Cardputer](https://shop.m5stack.com/). 

This project bridges the gap between your cloud-based or local AI coding assistant and the physical world. It provides a tactile, glanceable, and voice-enabled interface to monitor, manage, and interact with Codex sessions without switching windows on your main workstation.

- **Codex Buddy** — A live dashboard showing your current workspace, active branch, running threads, and real-time status (Idle, Busy, Waiting for Approval).
- **Push to Codex** — Hold SPACE to record a voice prompt. The device streams audio to the host middleware, where `faster-whisper` transcribes it and forwards the text to Codex. Perfect for quick architectural queries.
- **Codex Pager** — A three-pane interface (Compose, Inbox, Detail) to manage long-running AI tasks. Read live terminal output, approve pending tool calls, send follow-up replies, or issue a hardware `/interrupt` to halt a runaway agent.
- **Codex Usage** — High-resolution telemetry screens tracking your primary (5h) and secondary (Weekly) rate limits, including human-readable reset countdowns.
- **MCP Bridge** — Turn the Cardputer into a pocket pager for any Model Context Protocol (MCP) compatible agent. The agent can send notifications, ask multiple-choice questions, or demand a physical confirmation gesture.

## Architecture

Unlike typical MicroPython setups, this project uses a high-performance **C++ Firmware** compiled with PlatformIO, paired with a robust **Python Middleware** running on your host machine (Windows, Mac, or Linux).

- **Firmware (`firmware/`)**: A highly responsive, state-driven UI built on Arduino/ESP-IDF. It uses an **Epoch-based Reactive Sync** model—it doesn't poll; it instantly updates when the host pushes new state via WebSockets.
- **Middleware (`middleware/`)**: A Python 3.11+ service managed by `uv`. It acts as the brain, bridging the Cardputer's WebSocket connection to the Codex `app-server` via `stdio`. It handles heavy lifting like mDNS broadcast, audio transcription, and state orchestration.

---

## Buy a Cardputer

The bundle targets the **M5Stack Cardputer** (ESP32-S3). Get one direct from [shop.m5stack.com](https://shop.m5stack.com/).

*Note: The built-in PDM microphone is required to use the "Push to Codex" voice transcription feature.*

---

## Quick Start — The Middleware (Host)

The middleware runs on your laptop or workstation. It manages the connection to Codex and serves the Cardputer over your local Wi-Fi.

### Prerequisites
- Python 3.11+
- [`uv`](https://docs.astral.sh/uv/) (Astral's fast Python package installer)

### Setup & Run
1. Navigate to the middleware directory and sync dependencies:
   ```bash
   cd middleware
   uv sync
   ```
   *(This downloads required packages, including `faster-whisper` models for voice transcription.)*

2. Start the bridge service:
   ```bash
   uv run cardputer-codex-middleware --serve --real-codex --host 0.0.0.0
   ```

**Features enabled automatically:**
- **mDNS Discovery:** The middleware broadcasts `_cardputer-codex._tcp.local.`. If your Cardputer is on the same Wi-Fi, it will find the host automatically—no IP configuration required!
- **Voice Transcription:** `faster-whisper` runs in a background thread to process audio chunks instantly.

---

## Quick Start — The Firmware (Device)

The firmware is written in C++ and built using PlatformIO.

### Prerequisites
- Python 3.x
- [PlatformIO Core (CLI)](https://docs.platformio.org/en/latest/core/index.html) or the VSCode Extension.

### Build & Flash
1. Plug your Cardputer into your computer via USB-C.
2. Navigate to the firmware directory and build/upload:
   ```bash
   cd firmware
   python -m platformio run --target upload
   ```
3. *(Optional)* Monitor the serial output to watch the boot process:
   ```bash
   python -m platformio device monitor
   ```

### Wi-Fi Configuration
On first boot, the device needs to know your Wi-Fi credentials. 
Currently, you must edit `firmware/src/device_config.h` (or provide an SD card configuration) to set your SSID and Password. 

Once connected to Wi-Fi, the device will use mDNS to automatically locate the middleware running on your host machine.

---

## Using the UI

The Cardputer uses a "Menu-First" navigation system, relying heavily on the keyboard.

### Global Shortcuts
- **`Ctrl + M`**: Open the main App Menu to switch between Buddy, Pager, Voice, etc.
- **`Fn + ;` / `Fn + .`**: Move selection Up / Down (mapped to avoid alpha-key conflicts).
- **`Enter`**: Select, Approve, or Send.
- **`Del`**: Back, Reject, or Cancel.
- **`/`**: Open the Command Palette from any screen.

### The Pager Flow
The Pager app has three views:
1. **Inbox**: Shows your recent threads. `Up/Down` to scroll (with smooth virtual windowing). `Enter` opens the Detail view.
2. **Detail**: Shows the live terminal output and event history for a specific thread.
3. **Compose/Reply**: Press `/reply` or `/compose` to type a new task.

**Stopping an Agent:** If Codex goes off the rails, open the Command Palette (`/`) and type `/interrupt` to immediately halt the active thread.

---

## Advanced: MCP Bridge

You can run the middleware as an MCP (Model Context Protocol) server instead of a direct Codex bridge. This allows any MCP-compliant client (Cursor, Claude Desktop, etc.) to ping your Cardputer.

Run the middleware with the `--mcp` flag:
```bash
uv run cardputer-codex-middleware --mcp
```
*Note: You must register the middleware's python executable and `cli.py` script within your respective MCP client's configuration.*

---

## 🛠 Working with Local Projects

The Cardputer Codex Terminal is designed to live next to your code. Here is how to use it with a specific project on your computer:

### 1. Launch the Middleware for a Project
Navigate to your project's root folder and point the middleware to it:
```bash
# From the cardputer-codex-terminal/middleware directory
uv run cardputer-codex-middleware --serve --workspace /path/to/your/awesome-code --real-codex --host 0.0.0.0
```
The middleware will automatically initialize a Codex session for that directory. Your Cardputer will instantly reflect the project name and active branch in the **Codex Buddy** dashboard.

### 2. Switching Branches
If you switch branches on your computer, the middleware pushes the update to the Cardputer automatically. You can also force a branch switch from the Cardputer:
1. Open the Command Palette (`/`).
2. Type `/branch feature/new-logic`.
3. Codex will re-sync to that specific git ref.

---

## 🔌 Two-Way Model Context Protocol (MCP)

The Cardputer Codex Terminal is a **two-way bridge**. Not only can you control Codex from your pocket, but Codex (or any other AI agent) can reach out and interact with you using the Model Context Protocol.

### 1. Complete Mode (`--mcp`)
When you run the middleware with the `--mcp` flag, it enables both the Cardputer WebSocket bridge and the MCP server simultaneously. This is the recommended way to use the project.

```bash
uv run cardputer-codex-middleware --mcp --host 0.0.0.0
```

### 2. Registering with Agents
Add the following to your MCP settings (e.g., `claude_desktop_config.json`):

```json
{
  "mcpServers": {
    "cardputer": {
      "command": "uv",
      "args": [
        "--project", "/path/to/cardputer-codex-terminal/middleware",
        "run", "cardputer-codex-middleware", "--mcp"
      ]
    }
  }
}
```

### 3. Agent-to-Device Tools
Once registered, the host-side Codex agent can call these physical tools:
- **`notify(title, body)`**: Flash a banner and chirp the speaker. Use this for "Build Finished" or "Tests Failed" alerts.
- **`ask(question, options)`**: Codex pauses and waits for you to pick an option on the Cardputer keyboard.
- **`confirm(title)`**: Forces a physical keypress on the hardware before Codex is allowed to run a destructive command.

---

## 🎙 Voice Transcription Deep Dive

To use real voice transcription, you must ensure the `faster-whisper` models are downloaded:

1. **Initial Sync**: `cd middleware && uv sync`
2. **On-Device**: Switch to **Push to Codex** (`Ctrl+M`).
3. **Capture**: Hold **SPACE**. The status bar will show "Recording..." and stream audio in real-time.
4. **Processing**: Release **SPACE**. The "Activity Spinner" in the header will spin while the host transcribes.
5. **Result**: The transcribed text appears on your screen and is automatically sent to Codex.

---

## 🎛 Command Palette Reference

Press `/` at any time to issue direct instructions to the system:

| Command | Action |
| :--- | :--- |
| `/app <name>` | Switch between `buddy`, `push`, `pager`, `usage`, `mcp`, `settings`. |
| `/interrupt` | Instantly kills the running Codex turn. |
| `/usage <0-100>` | Manually set usage level (for testing UI). |
| `/wifi reload` | Force re-scan of the SD card `config.ini`. |
| `/status <text>` | Set a custom status message on the header. |

---

## Troubleshooting

- **"Middleware disconnected" / "Auto-discovering bridge..."**
  Ensure your host machine and the Cardputer are on the same Wi-Fi network. Check your host's firewall settings to ensure port `8765` and mDNS (`5353/UDP`) are not blocked.
- **"Message too big" (1009 Error)**
  This should be resolved by the Epoch-based sync model, but ensure you are running the latest firmware build, which increases `WEBSOCKETS_MAX_DATA_SIZE` to 16KB.
- **Voice Transcription hangs**
  Make sure you ran `uv sync` to download the `faster-whisper` dependencies. If you are on a highly constrained host, the middleware automatically falls back to a Mock transcriber if the library is missing.

## License

This project's code is licensed under **MIT** (Middleware) and **Apache 2.0** (Firmware where applicable). See individual directory notices for specifics.
