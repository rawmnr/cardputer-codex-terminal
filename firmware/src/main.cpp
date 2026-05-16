#include <Arduino.h>
#include <M5Cardputer.h>

#include "app_shell.h"

namespace {
AppShell g_shell;

void poll_keyboard_input() {
  M5Cardputer.update();

  if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) {
    return;
  }

  const auto status = M5Cardputer.Keyboard.keysState();
  String typed;
  for (auto ch : status.word) {
    typed += ch;
  }

  if (status.del) {
    g_shell.handleKeyboardInput("", false, true);
  }

  if (status.enter) {
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
