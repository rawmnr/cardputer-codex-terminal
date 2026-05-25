#pragma once

#include <Arduino.h>

class M5Canvas : public Print {
 public:
  explicit M5Canvas(void*) {}

  size_t write(uint8_t) override { return 1; }

  template <typename... Args>
  void setColorDepth(Args...) {}
  template <typename... Args>
  void setTextSize(Args...) {}
  template <typename... Args>
  void setTextWrap(Args...) {}
  template <typename... Args>
  void setTextScroll(Args...) {}
  template <typename... Args>
  void createSprite(Args...) {}
  template <typename... Args>
  void deleteSprite(Args...) {}
  template <typename... Args>
  void pushSprite(Args...) {}
  template <typename... Args>
  void fillSprite(Args...) {}
  template <typename... Args>
  void setTextColor(Args...) {}
  template <typename... Args>
  void setCursor(Args...) {}
  template <typename... Args>
  void fillRoundRect(Args...) {}
  template <typename... Args>
  void drawRoundRect(Args...) {}
  template <typename... Args>
  void fillRect(Args...) {}
  template <typename... Args>
  void drawRect(Args...) {}
  template <typename... Args>
  void fillCircle(Args...) {}
  template <typename... Args>
  void pushImage(Args...) {}
};

class M5CardputerClass {
 public:
  struct DisplayClass : public Print {
    size_t write(uint8_t) override { return 1; }

    template <typename... Args>
    void setRotation(Args...) {}
    template <typename... Args>
    void fillScreen(Args...) {}
    template <typename... Args>
    void setTextSize(Args...) {}
    template <typename... Args>
    void fillRect(Args...) {}
    template <typename... Args>
    void setTextColor(Args...) {}
    template <typename... Args>
    void setCursor(Args...) {}
    template <typename... Args>
    void fillRoundRect(Args...) {}
    template <typename... Args>
    void fillCircle(Args...) {}
    template <typename... Args>
    void drawRect(Args...) {}
    template <typename... Args>
    void drawRoundRect(Args...) {}
    template <typename... Args>
    void startWrite(Args...) {}
    template <typename... Args>
    void pushImage(Args...) {}
    template <typename... Args>
    void endWrite(Args...) {}
    template <typename... Args>
    void setSwapBytes(Args...) {}
    template <typename... Args>
    void setBrightness(Args...) {}
  } Display;

  struct KeyboardState {
    String word;
    bool space = false;
    bool tab = false;
    bool enter = false;
    bool del = false;
    bool ctrl = false;
    bool fn = false;
    bool change = false;
    bool pressed = false;

    bool isChange() const { return change; }
    bool isPressed() const { return pressed; }
  };

  class KeyboardClass {
   public:
    KeyboardState keysState() const { return state_; }
    bool isChange() const { return state_.isChange(); }
    bool isPressed() const { return state_.isPressed(); }

    KeyboardState state_;
  } Keyboard;

  class PowerClass {
   public:
    int getBatteryLevel() const { return 100; }
    int getBatteryVoltage() const { return 4100; }
  } Power;

  class MicClass {
   public:
    bool isEnabled() const { return true; }
    void setSampleRate(int) {}
    bool begin() { return true; }
    template <typename T>
    bool record(T*, size_t, int) { return false; }
  } Mic;

  struct Config {
    int dummy = 0;
  };

  Config config() const { return {}; }

  template <typename... Args>
  void begin(Args...) {}
  void update() {}
};

extern M5CardputerClass M5Cardputer;
extern M5CardputerClass M5;
extern WiFiClass WiFi;
extern SerialMock Serial;
extern SDClass SD;
extern MDNSClass MDNS;
extern SPIClass SPI;
