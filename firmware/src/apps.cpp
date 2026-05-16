#include "apps.h"

namespace {
const char* app_label(AppId app_id) {
  switch (app_id) {
    case AppId::Buddy:
      return "Codex Buddy";
    case AppId::PushToCodex:
      return "Push to Codex";
    case AppId::Pager:
      return "Codex Pager";
    case AppId::McpBridge:
      return "Cardputer MCP Bridge";
  }
  return "Unknown";
}

void print_common_footer(Print& out) {
  out.println();
  out.println("Commands: /app buddy|push|pager|mcp | /wifi on|off | /codex idle|busy|approval|offline");
  out.println("          /battery <0-100> | /status <text> | /help");
}
}  // namespace

const char* BuddyApp::title() const { return "Codex Buddy"; }

void BuddyApp::onEnter(DeviceState& state) {
  state.status_line = "Buddy mode active";
}

void BuddyApp::onExit(DeviceState& state) {
  (void)state;
}

void BuddyApp::onCommand(const String& command, DeviceState& state) {
  (void)command;
  state.status_line = "Buddy view ready for live status";
}

void BuddyApp::tick(DeviceState& state) {
  (void)state;
}

void BuddyApp::render(Print& out, const DeviceState& state) {
  out.println("=== Codex Buddy ===");
  out.print("Wi-Fi: ");
  out.println(state.wifi_connected ? "connected" : "offline");
  out.print("Net: ");
  out.println(state.network_status_line);
  out.print("Codex: ");
  switch (state.codex_state) {
    case CodexState::Offline:
      out.println("offline");
      break;
    case CodexState::Idle:
      out.println("idle");
      break;
    case CodexState::Busy:
      out.println("busy");
      break;
    case CodexState::WaitingForApproval:
      out.println("waiting for approval");
      break;
  }
  out.print("Battery: ");
  out.print(state.battery_percent);
  out.println("%");
  out.print("Status: ");
  out.println(state.status_line);
  print_common_footer(out);
}

const char* PushToCodexApp::title() const { return "Push to Codex"; }

void PushToCodexApp::onEnter(DeviceState& state) {
  draft_ = "";
  state.status_line = "Compose a prompt and send it to Codex";
}

void PushToCodexApp::onExit(DeviceState& state) {
  (void)state;
}

void PushToCodexApp::onCommand(const String& command, DeviceState& state) {
  draft_ = command;
  state.status_line = "Prompt staged for delivery";
}

void PushToCodexApp::onSubmit(const String& command, DeviceState& state) {
  draft_ = command;
  state.status_line = "Prompt submitted to middleware bridge";
}

void PushToCodexApp::tick(DeviceState& state) {
  (void)state;
}

void PushToCodexApp::render(Print& out, const DeviceState& state) {
  out.println("=== Push to Codex ===");
  out.print("Draft: ");
  out.println(draft_.length() > 0 ? draft_ : "(empty)");
  out.print("Net: ");
  out.println(state.network_status_line);
  out.print("Codex state: ");
  switch (state.codex_state) {
    case CodexState::Offline:
      out.println("offline");
      break;
    case CodexState::Idle:
      out.println("idle");
      break;
    case CodexState::Busy:
      out.println("busy");
      break;
    case CodexState::WaitingForApproval:
      out.println("waiting for approval");
      break;
  }
  out.print("Status: ");
  out.println(state.status_line);
  out.println("Type a prompt, then forward it to the middleware in a later step.");
  print_common_footer(out);
}

const char* PagerApp::title() const { return "Codex Pager"; }

void PagerApp::onEnter(DeviceState& state) {
  state.status_line = "Pager mode active";
}

void PagerApp::onExit(DeviceState& state) {
  (void)state;
}

void PagerApp::onCommand(const String& command, DeviceState& state) {
  (void)command;
  state.status_line = "Pager inbox and session detail will arrive next";
}

void PagerApp::tick(DeviceState& state) {
  (void)state;
}

void PagerApp::render(Print& out, const DeviceState& state) {
  out.println("=== Codex Pager ===");
  out.print("Active app: ");
  out.println(app_label(state.active_app));
  out.print("Net: ");
  out.println(state.network_status_line);
  out.println("Inbox, session detail, interrupts, and approvals will land here.");
  out.print("Status: ");
  out.println(state.status_line);
  print_common_footer(out);
}

const char* McpBridgeApp::title() const { return "Cardputer MCP Bridge"; }

void McpBridgeApp::onEnter(DeviceState& state) {
  state.status_line = "Local bridge mode active";
}

void McpBridgeApp::onExit(DeviceState& state) {
  (void)state;
}

void McpBridgeApp::onCommand(const String& command, DeviceState& state) {
  (void)command;
  state.status_line = "Notification, ask, and confirm flows will attach here";
}

void McpBridgeApp::tick(DeviceState& state) {
  (void)state;
}

void McpBridgeApp::render(Print& out, const DeviceState& state) {
  out.println("=== Cardputer MCP Bridge ===");
  out.println("Planned tools: notify, ask, confirm.");
  out.print("Status: ");
  out.println(state.status_line);
  print_common_footer(out);
}
