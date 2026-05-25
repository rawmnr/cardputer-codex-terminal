#include <Arduino.h>
#include <M5Cardputer.h>

#include "runtime/firmware_runtime.h"

namespace {
FirmwareRuntime g_runtime;
}

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  Serial.begin(115200);
  delay(200);

  g_runtime.begin();

  Serial.println();
  Serial.println("cardputer-codex-terminal firmware shell ready");
  Serial.println("Type /help for commands.");
}

void loop() {
  g_runtime.loop();
}
