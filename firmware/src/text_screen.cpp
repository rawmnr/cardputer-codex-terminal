#include "text_screen.h"

void TextScreen::begin() {
  Serial.println();
}

void TextScreen::renderShell(const DeviceState& state, App& app) {
  Serial.println();
  drawHeader(state);
  app.render(Serial, state);
}

void TextScreen::drawHeader(const DeviceState& state) {
  Serial.println("========================================");
  Serial.print("Firmware: ");
  Serial.println(state.firmware_name);
  Serial.print("App: ");
  switch (state.active_app) {
    case AppId::Buddy:
      Serial.println("Codex Buddy");
      break;
    case AppId::PushToCodex:
      Serial.println("Push to Codex");
      break;
    case AppId::Pager:
      Serial.println("Codex Pager");
      break;
    case AppId::McpBridge:
      Serial.println("Cardputer MCP Bridge");
      break;
  }
  Serial.print("Network: ");
  Serial.println(state.wifi_connected ? "online" : "offline");
  Serial.print("Battery: ");
  Serial.print(state.battery_percent);
  Serial.println("%");
  Serial.print("Codex: ");
  switch (state.codex_state) {
    case CodexState::Offline:
      Serial.println("offline");
      break;
    case CodexState::Idle:
      Serial.println("idle");
      break;
    case CodexState::Busy:
      Serial.println("busy");
      break;
    case CodexState::WaitingForApproval:
      Serial.println("approval pending");
      break;
  }
  drawDivider();
}

void TextScreen::drawDivider() {
  Serial.println("========================================");
}

