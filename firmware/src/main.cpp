#include <Arduino.h>
#include <M5Cardputer.h>

#include "app_shell.h"

namespace {
AppShell g_shell;
bool g_last_space_state = false;
bool g_space_hold_started = false;
unsigned long g_space_pressed_at_ms = 0;
constexpr unsigned long kPushToTalkHoldMs = 350;

void poll_keyboard_input() {
  M5Cardputer.update();

  if (!M5Cardputer.Keyboard.isChange()) {
    const auto status = M5Cardputer.Keyboard.keysState();
    const bool push_mode = g_shell.isPushToCodexActive();
    if (push_mode && status.space && !g_space_hold_started && g_space_pressed_at_ms > 0) {
      const unsigned long held_ms = millis() - g_space_pressed_at_ms;
      if (held_ms >= kPushToTalkHoldMs) {
        g_shell.handlePushToTalk(true);
        g_space_hold_started = true;
      }
    }
    return;
  }

  const auto status = M5Cardputer.Keyboard.keysState();
  const bool push_mode = g_shell.isPushToCodexActive();
  String typed;
  for (auto ch : status.word) {
    if (ch == ' ' && push_mode) {
      continue;
    }
    typed += ch;
  }

  if (status.space != g_last_space_state) {
    g_last_space_state = status.space;
    if (push_mode) {
      if (status.space) {
        g_space_pressed_at_ms = millis();
        g_space_hold_started = false;
      } else {
        if (g_space_hold_started) {
          g_shell.handlePushToTalk(false);
        } else {
          g_shell.handleKeyboardInput(" ", false, false);
        }
        g_space_pressed_at_ms = 0;
        g_space_hold_started = false;
      }
    }
  }

  if (!M5Cardputer.Keyboard.isPressed()) {
    return;
  }

  if (status.del) {
    if (typed.length() == 0 && g_shell.hasPendingApproval()) {
      g_shell.handleApprovalDecision(false);
      return;
    }
    if (typed.length() == 0 && g_shell.hasPendingBridgePrompt()) {
      g_shell.handleBridgePromptDecision(false);
      return;
    }
    g_shell.handleKeyboardInput("", false, true);
  }

  if (status.enter) {
    if (typed.length() == 0 && g_shell.hasPendingApproval()) {
      g_shell.handleApprovalDecision(true);
      return;
    }
    if (typed.length() == 0 && g_shell.hasPendingBridgePrompt()) {
      g_shell.handleBridgePromptDecision(true);
      return;
    }
    g_shell.handleKeyboardInput(typed, true, false);
    return;
  }

  if (typed.length() > 0) {
    g_shell.handleKeyboardInput(typed, false, false);
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
