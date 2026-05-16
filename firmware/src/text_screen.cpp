#include "text_screen.h"

void TextScreen::begin() {
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.fillScreen(BLACK);

  canvas_.setColorDepth(16);
  canvas_.setTextSize(1);
  canvas_.setTextWrap(true);
  canvas_.setTextScroll(true);
  canvas_.createSprite(M5Cardputer.Display.width() - 8, M5Cardputer.Display.height() - 64);
  canvas_.fillSprite(BLACK);
}

void TextScreen::renderShell(const DeviceState& state, App& app, const String& input_line) {
  M5Cardputer.Display.fillScreen(BLACK);
  drawHeader(state);
  canvas_.fillSprite(BLACK);
  app.render(canvas_, state);
  canvas_.pushSprite(4, 42);
  drawInputLine(input_line);
}

void TextScreen::drawHeader(const DeviceState& state) {
  M5Cardputer.Display.fillRect(0, 0, M5Cardputer.Display.width(), 20, DARKGREY);
  M5Cardputer.Display.setTextColor(WHITE, DARKGREY);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.drawString(state.firmware_name.c_str(), 4, 4);
  const String network_label = state.wifi_connected ? String("Wi-Fi") : String("Off");
  M5Cardputer.Display.drawString(network_label.c_str(), M5Cardputer.Display.width() - 48, 4);
  M5Cardputer.Display.setTextColor(WHITE, BLACK);
  M5Cardputer.Display.fillRect(0, 20, M5Cardputer.Display.width(), M5Cardputer.Display.height() - 20, BLACK);
  M5Cardputer.Display.drawRect(0, 42, M5Cardputer.Display.width(), M5Cardputer.Display.height() - 62, DARKGREY);
  M5Cardputer.Display.drawString("App:", 4, 24);
  switch (state.active_app) {
    case AppId::Buddy:
      M5Cardputer.Display.drawString("Codex Buddy", 40, 24);
      break;
    case AppId::PushToCodex:
      M5Cardputer.Display.drawString("Push to Codex", 40, 24);
      break;
    case AppId::Pager:
      M5Cardputer.Display.drawString("Codex Pager", 40, 24);
      break;
    case AppId::McpBridge:
      M5Cardputer.Display.drawString("Cardputer MCP Bridge", 40, 24);
      break;
  }
  M5Cardputer.Display.drawString("Battery:", 4, 34);
  M5Cardputer.Display.setCursor(60, 34);
  M5Cardputer.Display.printf("%3d%%", state.battery_percent);
  M5Cardputer.Display.drawString("mV", 92, 34);
  M5Cardputer.Display.setCursor(112, 34);
  M5Cardputer.Display.printf("%4d", state.battery_voltage_mv);
  M5Cardputer.Display.drawString("Cdx:", 172, 34);
  switch (state.codex_state) {
    case CodexState::Offline:
      M5Cardputer.Display.drawString("off", 206, 34);
      break;
    case CodexState::Idle:
      M5Cardputer.Display.drawString("idl", 206, 34);
      break;
    case CodexState::Busy:
      M5Cardputer.Display.drawString("bsy", 206, 34);
      break;
    case CodexState::WaitingForApproval:
      M5Cardputer.Display.drawString("app", 206, 34);
      break;
  }
}

void TextScreen::drawInputLine(const String& input_line) {
  M5Cardputer.Display.fillRect(0, M5Cardputer.Display.height() - 20, M5Cardputer.Display.width(), 20, DARKGREY);
  M5Cardputer.Display.setTextColor(WHITE, DARKGREY);
  M5Cardputer.Display.drawString((String("> ") + input_line).c_str(), 4, M5Cardputer.Display.height() - 16);
}
