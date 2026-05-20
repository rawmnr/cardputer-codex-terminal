#pragma once

#include <Arduino.h>

class M5Canvas : public Print {
 public:
  M5Canvas(void*) {}
  void createSprite(int, int) {}
  void deleteSprite() {}
  void pushSprite(int, int) {}
  void fillSprite(uint16_t) {}
  void setTextColor(uint16_t) {}
  void setCursor(int, int) {}
  void printf(const char*, ...) {}
  void println(const char*) {}
  void setTextFont(int) {}
  void setTextSize(int) {}
  size_t write(uint8_t) override { return 1; }
};

class M5CardputerClass {
 public:
  struct Display {
    void setBrightness(uint8_t) {}
    int width() { return 240; }
    int height() { return 135; }
  };
  struct Power {
    int getBatteryLevel() { return 85; }
    int getBatteryVoltage() { return 4000; }
  };
  struct Mic {
    bool isEnabled() { return true; }
    bool begin() { return true; }
    void setSampleRate(int) {}
    bool record(void*, size_t, int) { return false; }
  };
  Display Display;
  Power Power;
  Mic Mic;
  void begin(bool = true, bool = true) {}
  void update() {}
};

extern M5CardputerClass M5Cardputer;
extern M5CardputerClass M5;
