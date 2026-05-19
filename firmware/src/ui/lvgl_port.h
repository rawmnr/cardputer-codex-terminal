#pragma once

#include <Arduino.h>

#ifndef USE_LVGL_UI
#define USE_LVGL_UI 0
#endif

#if USE_LVGL_UI
#include <lvgl.h>

class LvglPort {
 public:
  void begin();
  void tick();
  void pushKey(lv_key_t key, bool pressed);
  bool ready() const;
  lv_group_t* group() const;

 private:
  struct KeyEvent {
    lv_key_t key = static_cast<lv_key_t>(0);
    lv_indev_state_t state = LV_INDEV_STATE_RELEASED;
  };

  static constexpr int kScreenWidth = 240;
  static constexpr int kScreenHeight = 135;
  static constexpr int kBufferLines = 20;
  static constexpr size_t kBufferPixels = static_cast<size_t>(kScreenWidth) * kBufferLines;
  static constexpr size_t kKeyQueueSize = 16;

  static void flushCb(lv_display_t* display, const lv_area_t* area, uint8_t* px_map);
  static void readCb(lv_indev_t* indev, lv_indev_data_t* data);

  void flushArea(const lv_area_t* area, const uint8_t* px_map);
  bool popKey(KeyEvent& event);
  void pushKeyEvent(lv_key_t key, lv_indev_state_t state);

  lv_display_t* display_ = nullptr;
  lv_indev_t* keypad_ = nullptr;
  lv_group_t* group_ = nullptr;
  std::array<lv_color_t, kBufferPixels> buffer_{};
#ifndef ARDUINO
  std::array<uint16_t, kScreenWidth * kScreenHeight> full_framebuffer_{};
#endif
  std::array<KeyEvent, kKeyQueueSize> key_queue_{};
  size_t key_head_ = 0;
  size_t key_tail_ = 0;
  unsigned long last_tick_ms_ = 0;

 public:
#ifndef ARDUINO
  const uint16_t* framebuffer() const { return full_framebuffer_.data(); }
#endif
};
#else
class LvglPort {
 public:
  void begin() {}
  void tick() {}
  void pushKey(int, bool) {}
  bool ready() const { return true; }
};
#endif
