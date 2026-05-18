#pragma once

#include <Arduino.h>
#include "device_state.h"

#ifndef USE_LVGL_UI
#define USE_LVGL_UI 0
#endif

#if USE_LVGL_UI
#include <lvgl.h>

class PttWidget {
 public:
  void begin(lv_obj_t* parent);
  void setVisible(bool visible);
  void sync(const DeviceState& state, bool active_app_visible, bool modal_visible);
  bool visible() const;

 private:
  static const char* stateLabel(PushToTalkState state);
  static lv_color_t stateColor(PushToTalkState state);
  static String shortText(const String& value, size_t max_chars);

  lv_obj_t* panel_ = nullptr;
  lv_obj_t* title_ = nullptr;
  lv_obj_t* state_ = nullptr;
  lv_obj_t* detail_ = nullptr;
  lv_obj_t* samples_ = nullptr;
  lv_obj_t* amplitude_ = nullptr;
  bool visible_ = false;
  String last_title_;
  String last_state_;
  String last_detail_;
  String last_samples_;
  int last_amplitude_ = -1;
};
#else
class PttWidget {
 public:
  void begin(void*) {}
  void setVisible(bool) {}
  void sync(const DeviceState&, bool, bool) {}
  bool visible() const { return false; }
};
#endif
