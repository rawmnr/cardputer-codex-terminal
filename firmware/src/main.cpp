#include <Arduino.h>

#include "app_shell.h"

namespace {
AppShell g_shell;
String g_input_buffer;

void poll_serial_input() {
  while (Serial.available() > 0) {
    const char ch = static_cast<char>(Serial.read());
    if (ch == '\r') {
      continue;
    }
    if (ch == '\n') {
      if (g_input_buffer.length() > 0) {
        g_shell.handleCommand(g_input_buffer);
        g_input_buffer = "";
      }
      continue;
    }
    g_input_buffer += ch;
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  g_shell.begin();
  g_shell.render();

  Serial.println();
  Serial.println("cardputer-codex-terminal firmware shell ready");
  Serial.println("Type /help for commands.");
}

void loop() {
  poll_serial_input();
  g_shell.tick();
  delay(16);
}

