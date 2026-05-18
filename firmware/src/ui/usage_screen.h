#pragma once

#include <Arduino.h>
#include "device_state.h"
#include "lvgl_app_screen.h"

#ifndef USE_LVGL_UI
#define USE_LVGL_UI 0
#endif

#if USE_LVGL_UI
#include <lvgl.h>

class UsageScreen final : public LvglAppScreen {
 public:
  void attach(lv_obj_t* parent, lv_group_t* group) override;
  void detach() override;
  void sync(const DeviceState& state) override;

 private:
  static void setLabelText(lv_obj_t* obj, String& cache, const String& value);
  static String shortText(const String& value, size_t max_chars);
  static void setBarColor(lv_obj_t* bar, int percent);

  lv_obj_t* root_ = nullptr;
  lv_obj_t* primary_title_ = nullptr;
  lv_obj_t* primary_bar_ = nullptr;
  lv_obj_t* primary_value_ = nullptr;
  lv_obj_t* secondary_title_ = nullptr;
  lv_obj_t* secondary_bar_ = nullptr;
  lv_obj_t* secondary_value_ = nullptr;
  lv_obj_t* reset_ = nullptr;
  String last_primary_title_;
  String last_primary_value_;
  String last_secondary_title_;
  String last_secondary_value_;
  String last_reset_;
};
#else
class UsageScreen final : public LvglAppScreen {
 public:
  void attach(void*, void*) {}
  void detach() {}
  void sync(const DeviceState&) {}
};
#endif
