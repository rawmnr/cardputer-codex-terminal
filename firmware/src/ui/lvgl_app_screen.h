#pragma once

#include <Arduino.h>

#ifndef USE_LVGL_UI
#define USE_LVGL_UI 0
#endif

#if USE_LVGL_UI
#include <lvgl.h>

#include "device_state.h"

class LvglAppScreen {
 public:
  virtual ~LvglAppScreen() = default;
  virtual void attach(lv_obj_t* parent, lv_group_t* group) = 0;
  virtual void detach() = 0;
  virtual void sync(const DeviceState& state) = 0;
  virtual void onFocus() {}
};
#else
class LvglAppScreen {
 public:
  virtual ~LvglAppScreen() = default;
};
#endif
