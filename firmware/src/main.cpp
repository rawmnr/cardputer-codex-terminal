#include <Arduino.h>
#include <M5Cardputer.h>

#include "app_shell.h"

namespace {
AppShell g_shell;
bool g_last_space_state = false;
bool g_space_hold_started = false;
unsigned long g_space_pressed_at_ms = 0;
constexpr unsigned long kPushToTalkHoldMs = 350;

bool mapNavigationChar(char ch, UiAction& action) {
  switch (ch) {
    case 'a':
    case 'A':
    case ',':
    case ';':
      action = UiAction::Left;
      return true;
    case 'd':
    case 'D':
    case '.':
    case '\'':
      action = UiAction::Right;
      return true;
    case 'w':
    case 'W':
      action = UiAction::Up;
      return true;
    case 's':
    case 'S':
      action = UiAction::Down;
      return true;
    default:
      return false;
  }
}

void sendTypedChar(char ch) {
  String typed;
  typed += ch;
  g_shell.handleTextInput(typed, false, false);
}

void poll_keyboard_input() {
  M5Cardputer.update();

  const auto status = M5Cardputer.Keyboard.keysState();
  const bool push_mode = g_shell.isPushToCodexActive();
  const bool input_mode = g_shell.uiMode() == UiMode::Input;

  if (!M5Cardputer.Keyboard.isChange()) {
    if (push_mode && status.space && !g_space_hold_started && g_space_pressed_at_ms > 0) {
      const unsigned long held_ms = millis() - g_space_pressed_at_ms;
      if (held_ms >= kPushToTalkHoldMs) {
        g_shell.handleAction(UiAction::PushToTalkStart);
        g_space_hold_started = true;
      }
    }
    return;
  }

  String typed;
  for (size_t i = 0; i < status.word.size(); ++i) {
    const char ch = status.word[i];
    if (push_mode && status.space && ch == ' ') {
      continue;
    }
    typed += ch;
  }

  if (status.space != g_last_space_state) {
    g_last_space_state = status.space;
    if (status.space) {
      g_space_pressed_at_ms = millis();
      g_space_hold_started = false;
    } else {
      if (push_mode && g_space_hold_started) {
        g_shell.handleAction(UiAction::PushToTalkStop);
      } else if (input_mode || push_mode) {
        g_shell.handleTextInput(" ", false, false);
      } else {
        g_shell.handleAction(UiAction::Select);
      }
      g_space_pressed_at_ms = 0;
      g_space_hold_started = false;
    }
  }

  if (!M5Cardputer.Keyboard.isPressed()) {
    return;
  }

  if (status.del) {
    if (input_mode || push_mode) {
      g_shell.handleTextInput("", false, true);
    } else {
      g_shell.handleAction(UiAction::Back);
    }
    return;
  }

  if (status.enter) {
    if (input_mode || push_mode) {
      g_shell.handleTextInput("", true, false);
    } else {
      g_shell.handleAction(UiAction::Select);
    }
    return;
  }

  if (typed.length() > 0) {
    if (input_mode || push_mode) {
      g_shell.handleTextInput(typed, false, false);
      return;
    }

    for (size_t i = 0; i < typed.length(); ++i) {
      const char ch = typed[i];
      UiAction action = UiAction::None;
      if (mapNavigationChar(ch, action)) {
        g_shell.handleAction(action);
      } else {
        sendTypedChar(ch);
      }
    }
  }
}
}  // namespace

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  Serial.begin(115200);
  delay(200);

  g_shell.begin();
  g_shell.render();

  Serial.println();
  Serial.println("cardputer-codex-terminal firmware shell ready");
  Serial.println("Type /help for commands.");
}

void loop() {
  poll_keyboard_input();
  g_shell.tick();
  delay(16);
}
