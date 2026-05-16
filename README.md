# cardputer-codex-terminal

Remote agent terminal for turning the M5Stack Cardputer ADV into a physical control surface for an OpenAI Codex instance running on Windows.

This repository is initialized as an architecture-first project. It does not yet contain firmware or middleware implementation code. The initial goal is to define the components, protocols, risks, and roadmap before implementation.

## Vision

The project connects a M5Stack Cardputer ADV to a Windows host running `codex app-server`, providing a mobile interface to:

- send text instructions from the Cardputer keyboard;
- dictate voice prompts via push-to-talk;
- follow Codex responses in streaming form;
- approve or reject requests;
- monitor long-running tasks remotely over a private overlay network.

## Target Architecture

```text
M5Stack Cardputer ADV
  | Wi-Fi + overlay VPN
  v
Python middleware on Windows
  | WebSocket / JSON-RPC
  v
OpenAI Codex app-server
  | local tools / MCP / shell
  v
Windows workspace
```

## Repository Structure

```text
docs/
  architecture.md          System overview
  hardware.md              Cardputer ADV hardware notes
  networking.md            Remote access, Tailscale, MicroLink
  middleware.md            Python bridge role and STT pipeline
  firmware.md              Embedded firmware design
  codex-app-server.md      JSON-RPC integration with Codex
  security.md              Threats, approvals, secrets
  roadmap.md               Delivery phases
  references.md            Sources and related projects
firmware/                  Future ESP32-S3 firmware
middleware/                Future Windows Python server
hardware/                  Notes, diagrams, pinout, technical assets
```

## Status

Phase 0: project scoping and repository structure.

Code will be added later, after the transport, security, and user experience choices are validated.
