# Security Policy

Cardputer Codex Terminal interacts with sensitive environments (local workstations and cloud-based AI agents). Maintaining the security of the bridge and the host machine is our highest priority.

## 🛡 Security Principles

Our architecture is built on the following principles:

*   **Zero Secret Exposure**: No secrets (API keys, bridge tokens) are ever stored in plaintext within the repository.
*   **Isolated Connectivity**: The Codex server is never directly exposed to the public internet; communication is brokered through the middleware.
*   **Explicit Approvals**: Destructive or irreversible actions always require a physical confirmation gesture from the user on the Cardputer hardware.
*   **Minimized Attack Surface**: The firmware is kept lightweight to reduce the complexity and potential vulnerability of the device.

## 🚨 Reporting a Vulnerability

If you discover a security vulnerability, please do not open a public issue. Instead, please report it privately by [opening an issue on GitHub with the `security` label](https://github.com/your-repo/issues/new?labels=security) (or follow the project's preferred private contact method).

We will investigate all reports promptly and work on a fix.

## ✅ Best Practices for Contributors

*   **Secure Coding**: When writing middleware, avoid logging sensitive information like prompts, transcription results, or bridge tokens.
*   **Validation**: Always validate input received from the Cardputer (e.g., command strings, payload sizes) to prevent buffer overflows or injection attacks.
*   **Authentication**: Always implement and test the `bridge_token` mechanism when designing new communication paths.
