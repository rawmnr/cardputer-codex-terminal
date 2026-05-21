#pragma once

#include <Arduino.h>

#include "apps.h"
#include "device_state.h"

void writeTerminalSnapshot(Print& out, const DeviceState& state, App* active_app, const String& input_line);
String buildTerminalSnapshot(const DeviceState& state, App* active_app, const String& input_line);
