# Contributing

Thank you for your interest in Cardputer Codex Terminal! This project is currently in the **architectural scoping phase**, and we welcome contributions that help solidify the foundation.

## 🎯 Focus Areas

During this phase, we are primarily looking for input and contributions in:

*   **Hardware Constraints**: Validating interaction patterns on the M5Stack Cardputer hardware.
*   **Protocol Validation**: Refining the communication between the firmware and the Codex app-server.
*   **Security Modeling**: Designing robust authentication and approval flows.
*   **Architecture**: Helping define the boundary between firmware and middleware.
*   **User Experience**: Refining the physical interface ergonomics and UX.

## 🛠 How to Contribute

### 1. Proposing Changes
Please open an **Issue** first to discuss any major architectural changes or new features. This ensures our efforts are aligned with the project's core principles (keeping the firmware lightweight and the middleware robust).

### 2. Pull Requests
Once a direction is agreed upon, you are welcome to submit a Pull Request. 

*   **Middleware**: Contributions are welcome in Python. Please ensure all tests pass with `cd middleware && uv sync --dev && uv run pytest tests -v`.
*   **Firmware**: Contributions are welcome in C++ (Arduino/PlatformIO). Please ensure the code compiles and adheres to the memory constraints of the ESP32-S3.

### 3. Development Workflow
*   **Middleware**: Use `uv` for dependency management.
*   **Firmware**: Use `PlatformIO` for builds and flashing.
*   **Documentation**: If you find a gap in the documentation, please submit a PR to fill it!

## 🤝 Code of Conduct
Please be respectful and constructive in all interactions. We aim to build a professional and collaborative environment.
