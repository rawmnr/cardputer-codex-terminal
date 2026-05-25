#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

constexpr int kCardputerProtocolVersion = 2;

String buildCardputerEnvelope(const String& id, const String& type, const JsonVariantConst& payload, const String& auth_token = "");
