#pragma once

#include <Arduino.h>

#include "apps.h"
#include "device_state.h"

class TextScreen {
 public:
  void begin();
  void renderShell(const DeviceState& state, App& app);

 private:
  void drawHeader(const DeviceState& state);
  void drawDivider();
};

