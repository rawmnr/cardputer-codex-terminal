#include "text_screen.h"

#include <cstdio>
#include <cstring>

namespace {
// Layout for the 240x135 landscape display.
constexpr int kScreenWidth = 240;
constexpr int kScreenHeight = 135;
constexpr int kHeaderH = 14;
constexpr int kFooterH = 12;
constexpr int kContentTop = kHeaderH;
constexpr int kContentH = kScreenHeight - kContentTop - kFooterH;
constexpr int kCanvasX = 4;
constexpr int kCanvasY = kContentTop + 3;
constexpr int kCanvasW = kScreenWidth - 8;
constexpr int kCanvasH = kContentH - 6;

// Palette.
constexpr uint16_t COL_BG = 0x0000;
constexpr uint16_t COL_HEADER = 0x1927;
constexpr uint16_t COL_FOOTER = 0x1927;
constexpr uint16_t COL_TEXT = 0xFFFF;
constexpr uint16_t COL_MUTED = 0x8CB5;
constexpr uint16_t COL_DIM = 0x52CD;
constexpr uint16_t COL_ACCENT = 0x06FF;
constexpr uint16_t COL_OK = 0x56F0;
constexpr uint16_t COL_WARN = 0xFE47;
constexpr uint16_t COL_ERR = 0xF28A;
constexpr uint16_t COL_APR = 0xFCA7;

struct TabInfo {
  AppId id;
  const char* label;
};

constexpr TabInfo kTabs[] = {
  {AppId::Buddy, "Buddy"},
  {AppId::PushToCodex, "Push"},
  {AppId::Pager, "Pager"},
  {AppId::McpBridge, "MCP"},
  {AppId::Settings, "Set"},
};

constexpr size_t kTabCount = sizeof(kTabs) / sizeof(kTabs[0]);

const char* appLongTitle(AppId id) {
  switch (id) {
    case AppId::Buddy:
      return "Codex Buddy";
    case AppId::PushToCodex:
      return "Push to Codex";
    case AppId::Pager:
      return "Codex Pager";
    case AppId::McpBridge:
      return "MCP Bridge";
    case AppId::Settings:
      return "Settings";
  }
  return "";
}

const char* menuLabel(AppId id) {
  switch (id) {
    case AppId::Buddy:
      return "Buddy";
    case AppId::PushToCodex:
      return "Push";
    case AppId::Pager:
      return "Pager";
    case AppId::McpBridge:
      return "MCP";
    case AppId::Settings:
      return "Set";
  }
  return "App";
}

uint16_t codexColor(CodexState state) {
  switch (state) {
    case CodexState::Offline:
      return COL_ERR;
    case CodexState::Idle:
      return COL_OK;
    case CodexState::Busy:
      return COL_WARN;
    case CodexState::WaitingForApproval:
      return COL_APR;
  }
  return COL_DIM;
}

const char* codexCode(CodexState state) {
  switch (state) {
    case CodexState::Offline:
      return "OFF";
    case CodexState::Idle:
      return "IDL";
    case CodexState::Busy:
      return "BSY";
    case CodexState::WaitingForApproval:
      return "APR";
  }
  return "---";
}

bool bridgeLooksConnected(const String& status) {
  return status.indexOf("connected") >= 0 ||
         status.indexOf("synchronized") >= 0 ||
         status.indexOf("ready") >= 0;
}

String truncate(const String& value, size_t max_chars) {
  if (value.length() <= max_chars) {
    return value;
  }
  if (max_chars <= 1) {
    return value.substring(0, max_chars);
  }
  return value.substring(0, max_chars - 1) + "~";
}
}  // namespace

void TextScreen::begin() {
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.fillScreen(COL_BG);
  M5Cardputer.Display.setTextSize(1);

  canvas_.setColorDepth(16);
  canvas_.setTextSize(1);
  canvas_.setTextWrap(false);
  canvas_.setTextScroll(false);
  canvas_.createSprite(kCanvasW, kCanvasH);
  canvas_.fillSprite(COL_BG);
}

void TextScreen::renderShell(const DeviceState& state, App& app, const String& input_line, const String& footer_hint) {
  drawHeader(state);

  M5Cardputer.Display.fillRect(0, kContentTop, kScreenWidth, kContentH, COL_BG);

  if (state.menu.app_menu_open) {
    drawAppMenu(state);
  } else {
    canvas_.fillSprite(COL_BG);
    canvas_.setCursor(0, 0);
    canvas_.setTextColor(COL_TEXT, COL_BG);
    app.render(canvas_, state);
    canvas_.pushSprite(kCanvasX, kCanvasY);
  }

  drawFooter(state, input_line, footer_hint);
}

void TextScreen::drawHeader(const DeviceState& state) {
  auto& d = M5Cardputer.Display;
  d.fillRect(0, 0, kScreenWidth, kHeaderH, COL_HEADER);

  if (state.approval_pending) {
    d.setTextColor(COL_APR, COL_HEADER);
    d.setCursor(4, 3);
    d.print("! APPROVAL NEEDED");
  } else if (state.bridge_prompt_pending) {
    d.setTextColor(COL_APR, COL_HEADER);
    d.setCursor(4, 3);
    d.print("! BRIDGE PROMPT");
  } else {
    d.setTextColor(COL_ACCENT, COL_HEADER);
    d.setCursor(4, 3);
    const char* title = state.menu.app_menu_open ? "APP MENU" : appLongTitle(state.active_app);
    d.print(truncate(title, 18));
  }

  int x = kScreenWidth - 4;

  char batBuf[8];
  snprintf(batBuf, sizeof(batBuf), "%d%%", state.battery_percent);
  const int batW = static_cast<int>(strlen(batBuf)) * 6;
  const uint16_t batCol = state.battery_percent > 30 ? COL_OK
                          : state.battery_percent > 15 ? COL_WARN
                                                       : COL_ERR;
  x -= batW;
  d.setTextColor(batCol, COL_HEADER);
  d.setCursor(x, 3);
  d.print(batBuf);

  x -= 6;
  const int pillW = 20;
  x -= pillW;
  const uint16_t pillCol = codexColor(state.codex_state);
  d.fillRoundRect(x, 2, pillW, 10, 2, pillCol);
  d.setTextColor(COL_BG, pillCol);
  d.setCursor(x + 2, 3);
  d.print(codexCode(state.codex_state));

  x -= 9;
  d.fillCircle(x + 2, 6, 2, bridgeLooksConnected(state.bridge_status_line) ? COL_OK : COL_DIM);

  x -= 9;
  d.fillCircle(x + 2, 6, 2, state.wifi_connected ? COL_OK : COL_ERR);
}

void TextScreen::drawAppMenu(const DeviceState& state) {
  canvas_.fillSprite(COL_BG);
  canvas_.fillRoundRect(0, 0, kCanvasW, kCanvasH, 5, COL_HEADER);
  canvas_.drawRoundRect(0, 0, kCanvasW, kCanvasH, 5, COL_DIM);
  canvas_.setTextColor(COL_TEXT, COL_HEADER);
  canvas_.setCursor(8, 6);
  canvas_.print("Applications");

  constexpr int row_h = 14;
  constexpr int start_y = 20;
  for (size_t i = 0; i < kTabCount; ++i) {
    const int row_y = start_y + static_cast<int>(i) * row_h;
    const bool selected = state.menu.app_menu_selected == i;
    if (selected) {
      canvas_.fillRoundRect(6, row_y - 1, kCanvasW - 12, row_h - 1, 3, COL_ACCENT);
      canvas_.setTextColor(COL_BG, COL_ACCENT);
    } else {
      canvas_.setTextColor(COL_TEXT, COL_HEADER);
    }

    canvas_.setCursor(10, row_y + 2);
    canvas_.print(menuLabel(kTabs[i].id));
  }

  canvas_.setTextColor(COL_MUTED, COL_HEADER);
  canvas_.setCursor(8, kCanvasH - 12);
  canvas_.print("W/S move  Enter open  Del close");
  canvas_.pushSprite(kCanvasX, kCanvasY);
}

void TextScreen::drawFooter(const DeviceState& state, const String& input_line, const String& footer_hint) {
  auto& d = M5Cardputer.Display;
  const int y0 = kScreenHeight - kFooterH;
  d.fillRect(0, y0, kScreenWidth, kFooterH, COL_FOOTER);

  const int text_y = y0 + 2;
  const size_t max_chars = static_cast<size_t>((kScreenWidth - 8) / 6);

  if (state.menu.command_palette_open) {
    d.setTextColor(COL_ACCENT, COL_FOOTER);
    d.setCursor(4, text_y);
    d.print('/');

    String shown = input_line;
    if (shown.length() > max_chars - 1) {
      shown = shown.substring(shown.length() - (max_chars - 1));
    }
    d.setTextColor(COL_TEXT, COL_FOOTER);
    d.setCursor(12, text_y);
    d.print(shown);

    const int caret_x = 12 + static_cast<int>(shown.length()) * 6;
    if ((millis() / 500) % 2 == 0) {
      d.fillRect(caret_x, text_y, 5, 8, COL_ACCENT);
    }
  } else {
    d.setTextColor(COL_MUTED, COL_FOOTER);
    d.setCursor(4, text_y);
    d.print(truncate(footer_hint, max_chars));
  }
}
