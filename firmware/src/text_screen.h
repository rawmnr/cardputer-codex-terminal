#pragma once

#include <Arduino.h>
#include <M5Cardputer.h>

#include "apps.h"
#include "device_state.h"

class TextScreen {
 public:
  void begin();
  void renderShell(const DeviceState& state, App& app, const String& input_line);

 private:
  void drawHeader(const DeviceState& state);
  void drawInputLine(const String& input_line);

  M5Canvas canvas_{&M5Cardputer.Display};
};
