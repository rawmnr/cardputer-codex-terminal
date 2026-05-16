#pragma once

#include <Arduino.h>
#include <WiFi.h>

#include "device_state.h"

class NetworkManager {
 public:
  void begin();
  void tick(DeviceState& state);

 private:
  void connect(DeviceState& state);

  unsigned long last_attempt_ms_ = 0;
  bool connect_in_progress_ = false;
};

