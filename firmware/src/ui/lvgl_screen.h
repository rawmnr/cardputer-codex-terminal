#pragma once

#include <Arduino.h>

#ifndef USE_LVGL_UI
#define USE_LVGL_UI 0
#endif

#if USE_LVGL_UI
#include <lvgl.h>

#include "apps.h"
#include "device_state.h"
#include "lvgl_port.h"

class LvglScreen {
 public:
  void begin();
  void renderShell(const DeviceState& state, App& app, const String& input_line, const String& footer_hint);
  void pushKey(lv_key_t key, bool pressed);
  void tick();

 private:
  static const char* const kTabMap[];

  static void onTabEvent(lv_event_t* event);

  LvglPort port_;
  lv_obj_t* root_ = nullptr;
  lv_obj_t* title_ = nullptr;
  lv_obj_t* active_app_ = nullptr;
  lv_obj_t* status_ = nullptr;
  lv_obj_t* detail_ = nullptr;
  lv_obj_t* wifi_ = nullptr;
  lv_obj_t* codex_ = nullptr;
  lv_obj_t* battery_ = nullptr;
  lv_obj_t* focus_ = nullptr;
  lv_obj_t* footer_ = nullptr;
  lv_obj_t* tabs_ = nullptr;
};
#else
#include "apps.h"
#include "device_state.h"
#include "lvgl_port.h"

class LvglScreen {
 public:
  void begin() {}
  void renderShell(const DeviceState&, App&, const String&, const String&) {}
  void pushKey(int, bool) {}
  void tick() {}
};
#endif
