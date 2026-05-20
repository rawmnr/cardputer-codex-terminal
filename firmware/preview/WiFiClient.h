#pragma once

#include <Arduino.h>

class WiFiClient {
 public:
  bool connect(const char*, uint16_t) { return true; }
  void stop() {}
};

class HTTPClient {
 public:
  void begin(const String&) {}
  int GET() { return 200; }
  String getString() { return "{}"; }
  void end() {}
};
