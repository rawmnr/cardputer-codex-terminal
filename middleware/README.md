# Cardputer Codex Middleware

The Windows bridge that connects the M5Stack Cardputer ADV to the OpenAI Codex ecosystem.

This middleware acts as a multi-faceted gateway, providing communication, protocol translation, and developer tooling to enable a physical terminal experience.

## 🚀 Architecture

The middleware orchestrates four primary roles:

1.  **WebSocket Bridge Server**: Provides a secure WebSocket interface for the Cardputer hardware. It handles message routing, session state management, and authentication via a `bridge_token`.
2.  **Codex Transport**: Manages the connection to the Codex app-server.
    *   **Production**: Uses `stdio` for high-reliability transport via the `StdioCodexAppServerTransport`.
    *   **Development**: Supports `websocket` transport for local loopback testing.
3.  **MCP Server**: Exposes the Cardputer as a set of Model Context Protocol (MCP) tools, allowing Codex to interact directly with the physical device (e.g., sending notifications, asking questions).
4.  **Developer Preview**: A web-based dashboard that mirrors the device's current session state, event stream, and screen content for real-time debugging.

---

## 🛠 Prerequisites

*   **Python 3.11+**
*   **[uv](https://github.com/astral-sh/uv)** (Python package and project manager)

---

## 📦 Installation

Navigate to the `middleware/` directory and sync the environment:

```bash
cd middleware
uv sync
```

---

## 🚦 Usage

### 1. Running the Bridge Server
To connect a physical Cardputer, start the WebSocket bridge. Use `CARDPUTER_BRIDGE_TOKEN` to secure remote access.

```bash
# Basic local serve
uv run cardputer-codex-middleware --serve

# Secure remote serve
CARDPUTER_BRIDGE_TOKEN=<shared-secret> uv run cardputer-codex-middleware --serve
```

### 2. Integrating with Real Codex
For production use, connect the middleware to the Codex app-server via the stable `stdio` transport.

```bash
# Run a quick prompt test
uv run cardputer-codex-middleware --real-codex --prompt "Hello Codex"

# Run the bridge server with real Codex integration
CARDPUTER_BRIDGE_TOKEN=<shared-secret> uv run cardputer-codex-middleware --serve --real-codex
```

### 3. Local Development & Preview
To debug interactions without physical hardware, use the mock/preview modes.

```bash
# Run the local developer preview dashboard
uv run cardputer-codex-middleware --preview
```
The preview dashboard is available at `http://127.0.0.1:8787/`.

### 4. Codex MCP Mode
Expose the Cardputer as an MCP server so Codex can use it as a toolset.

```bash
uv run cardputer-codex-middleware --mcp
```

**Example Codex MCP Configuration (`config.toml`):**
```toml
[mcp_servers.cardputer]
command = "uv"
args = ["run", "cardputer-codex-middleware", "--mcp"]
cwd = "C:\\path\\to\\cardputer-codex-terminal\\middleware"
tool_timeout_sec = 120
```

---

## 📜 Protocol & Contract

The Cardputer message contract is versioned to ensure compatibility between firmware and middleware.

*   **Current Protocol Version**: `2`
*   **Message Envelope**: All messages follow a structured JSON-RPC-style envelope.
*   **Reliability**: Requests include a unique `id`. The middleware responds with an `ack` frame before the semantic response to allow callers to track pending requests.

### Available MCP Tools
When running in MCP mode, the following tools are exposed:
*   `cardputer.notify(title, body, urgency)`: Send a notification to the device.
*   `cardputer.ask(question, choices, timeout_s)`: Prompt the user with multiple choice options.
*   `cardputer.confirm(title, detail, danger, timeout_s)`: Request explicit confirmation for critical actions.
*   `cardputer.show(text, channel)`: Display a short text message on a specific screen channel.
*   `cardputer.dictate(prompt, max_seconds)`: *(Planned)* Trigger voice dictation.

---

## 🔧 Tooling & Development

### Schema Generation
If the Codex app-server updates, regenerate the protocol schemas:
```bash
codex app-server generate-ts --out ./schemas
codex app-server generate-json-schema --out ./schemas
```

### Running Tests
The middleware is tested using `pytest`.

```bash
uv run pytest tests -v
```
