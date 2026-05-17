#include "text_screen.h"

#include <cstring>

namespace {
String trimToWidth(String value, size_t max_chars) {
  if (value.length() <= max_chars) {
    return value;
  }
  if (max_chars <= 3) {
    return value.substring(0, max_chars);
  }
  return value.substring(0, max_chars - 3) + "...";
}

const char* tabLabel(AppId app_id) {
  switch (app_id) {
    case AppId::Buddy:
      return "Buddy";
    case AppId::PushToCodex:
      return "Push";
    case AppId::Pager:
      return "Pager";
    case AppId::McpBridge:
      return "MCP";
    case AppId::Settings:
      return "Settings";
  }
  return "App";
}
}

void TextScreen::begin() {
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.fillScreen(BLACK);

  canvas_.setColorDepth(16);
  canvas_.setTextSize(1);
  canvas_.setTextWrap(true);
  canvas_.setTextScroll(true);
  canvas_.createSprite(M5Cardputer.Display.width() - 8, M5Cardputer.Display.height() - 60);
  canvas_.fillSprite(BLACK);
}

void TextScreen::renderShell(const DeviceState& state, App& app, const String& input_line, const String& footer_hint) {
  M5Cardputer.Display.fillScreen(BLACK);
  drawHeader(state);
  canvas_.fillSprite(BLACK);
  app.render(canvas_, state);
  canvas_.pushSprite(4, 42);
  String footer = footer_hint;
  if (input_line.length() > 0) {
    footer += footer.length() > 0 ? " | " : "";
    footer += input_line;
  }
  drawFooter(footer);
}

void TextScreen::drawHeader(const DeviceState& state) {
  M5Cardputer.Display.fillRect(0, 0, M5Cardputer.Display.width(), 38, DARKGREY);
  M5Cardputer.Display.setTextColor(WHITE, DARKGREY);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.drawString(state.firmware_name.c_str(), 4, 4);
  const String network_label = trimToWidth(state.network_status_line.length() > 0 ? state.network_status_line : String("Wi-Fi offline"), 24);
  M5Cardputer.Display.drawString(network_label.c_str(), 78, 4);
  M5Cardputer.Display.setTextColor(WHITE, DARKGREY);
  M5Cardputer.Display.setCursor(4, 18);
  M5Cardputer.Display.print("Tabs:");
  const AppId tabs[] = {AppId::Buddy, AppId::PushToCodex, AppId::Pager, AppId::McpBridge, AppId::Settings};
  int x = 42;
  for (size_t i = 0; i < sizeof(tabs) / sizeof(tabs[0]); ++i) {
    const char* label = tabLabel(tabs[i]);
    const int label_width = static_cast<int>(strlen(label) * 6 + 10);
    const bool active = state.menu.active_tab == i;
    if (active) {
      M5Cardputer.Display.fillRect(x - 2, 16, label_width, 12, BLACK);
      M5Cardputer.Display.setTextColor(WHITE, BLACK);
    } else {
      M5Cardputer.Display.setTextColor(WHITE, DARKGREY);
    }
    M5Cardputer.Display.drawString(label, x + 3, 18);
    x += label_width + 2;
  }
  M5Cardputer.Display.setTextColor(WHITE, BLACK);
  M5Cardputer.Display.fillRect(0, 38, M5Cardputer.Display.width(), M5Cardputer.Display.height() - 38, BLACK);
  M5Cardputer.Display.drawRect(0, 42, M5Cardputer.Display.width(), M5Cardputer.Display.height() - 60, DARKGREY);
  M5Cardputer.Display.drawString("App:", 4, 24);
  M5Cardputer.Display.drawString(tabLabel(state.active_app), 40, 24);
  M5Cardputer.Display.drawString("Battery:", 148, 24);
  M5Cardputer.Display.setCursor(204, 24);
  M5Cardputer.Display.printf("%3d%%", state.battery_percent);
  M5Cardputer.Display.drawString("Cdx:", 4, 32);
  if (state.approval_pending) {
    M5Cardputer.Display.drawString("apr", 40, 32);
  } else if (state.bridge_prompt_pending) {
    M5Cardputer.Display.drawString("brg", 40, 32);
  }
  switch (state.codex_state) {
    case CodexState::Offline:
      M5Cardputer.Display.drawString("off", 72, 32);
      break;
    case CodexState::Idle:
      M5Cardputer.Display.drawString("idl", 72, 32);
      break;
    case CodexState::Busy:
      M5Cardputer.Display.drawString("bsy", 72, 32);
      break;
    case CodexState::WaitingForApproval:
      M5Cardputer.Display.drawString("app", 72, 32);
      break;
  }
}

void TextScreen::drawFooter(const String& footer_text) {
  M5Cardputer.Display.fillRect(0, M5Cardputer.Display.height() - 18, M5Cardputer.Display.width(), 18, DARKGREY);
  M5Cardputer.Display.setTextColor(WHITE, DARKGREY);
  M5Cardputer.Display.drawString(footer_text.c_str(), 4, M5Cardputer.Display.height() - 14);
}
