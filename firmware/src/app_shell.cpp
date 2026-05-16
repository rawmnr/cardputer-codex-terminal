#include "app_shell.h"

namespace {
String trimmed_copy(const String& input) {
  String output = input;
  output.trim();
  return output;
}
}  // namespace

void AppShell::begin() {
  state_.firmware_name = "cardputer-codex-terminal";
  state_.active_app = AppId::Buddy;
  state_.battery_percent = 87;
  state_.wifi_connected = false;
  state_.codex_state = CodexState::Idle;
  state_.status_line = "Waiting for middleware connection";

  screen_.begin();
  switchTo(AppId::Buddy);
}

void AppShell::handleCommand(const String& command) {
  const String trimmed = trimmed_copy(command);
  if (trimmed.length() == 0) {
    return;
  }

  if (trimmed == "/help") {
    Serial.println("Commands:");
    Serial.println("  /app buddy|push|pager|mcp");
    Serial.println("  /wifi on|off");
    Serial.println("  /codex idle|busy|approval|offline");
    Serial.println("  /battery <0-100>");
    Serial.println("  /status <text>");
    Serial.println("  anything else is forwarded to the active app");
    return;
  }

  if (trimmed.startsWith("/app ")) {
    const String value = trimmed.substring(5);
    if (value == "buddy") {
      switchTo(AppId::Buddy);
    } else if (value == "push") {
      switchTo(AppId::PushToCodex);
    } else if (value == "pager") {
      switchTo(AppId::Pager);
    } else if (value == "mcp") {
      switchTo(AppId::McpBridge);
    } else {
      Serial.println("Unknown app.");
    }
    render();
    return;
  }

  if (trimmed.startsWith("/wifi ")) {
    state_.wifi_connected = trimmed.endsWith("on");
    state_.status_line = state_.wifi_connected ? "Wi-Fi connected" : "Wi-Fi offline";
    render();
    return;
  }

  if (trimmed.startsWith("/codex ")) {
    const String value = trimmed.substring(7);
    if (value == "idle") {
      state_.codex_state = CodexState::Idle;
    } else if (value == "busy") {
      state_.codex_state = CodexState::Busy;
    } else if (value == "approval") {
      state_.codex_state = CodexState::WaitingForApproval;
    } else if (value == "offline") {
      state_.codex_state = CodexState::Offline;
    }
    render();
    return;
  }

  if (trimmed.startsWith("/battery ")) {
    const int value = trimmed.substring(9).toInt();
    state_.battery_percent = constrain(value, 0, 100);
    render();
    return;
  }

  if (trimmed.startsWith("/status ")) {
    state_.status_line = trimmed.substring(8);
    render();
    return;
  }

  if (active_app_ != nullptr) {
    active_app_->onCommand(trimmed, state_);
    render();
  }
}

void AppShell::tick() {
  if (active_app_ != nullptr) {
    active_app_->tick(state_);
  }
}

void AppShell::render() {
  screen_.renderShell(state_, *active_app_);
}

void AppShell::switchTo(AppId app_id) {
  if (active_app_ != nullptr) {
    active_app_->onExit(state_);
  }

  state_.active_app = app_id;
  switch (app_id) {
    case AppId::Buddy:
      active_app_ = &buddy_app_;
      break;
    case AppId::PushToCodex:
      active_app_ = &push_to_codex_app_;
      break;
    case AppId::Pager:
      active_app_ = &pager_app_;
      break;
    case AppId::McpBridge:
      active_app_ = &mcp_bridge_app_;
      break;
  }

  if (active_app_ != nullptr) {
    active_app_->onEnter(state_);
  }
}

