#pragma once

#include <Arduino.h>
#include <M5Cardputer.h>

#include "apps.h"
#include "device_state.h"

class TextScreen {
 public:
  void begin();
  void renderShell(const DeviceState& state, App& app, const String& input_line, const String& footer_hint);

 private:
  void drawHeader(const DeviceState& state);
  void drawActivityIndicator(const DeviceState& state, int x, int y);
  void drawAppMenu(const DeviceState& state);
  void drawFooter(const DeviceState& state, const String& input_line, const String& footer_hint);

  M5Canvas canvas_{&M5Cardputer.Display};
};
