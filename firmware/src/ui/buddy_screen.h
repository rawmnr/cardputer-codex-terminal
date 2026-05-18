#pragma once

#include <Arduino.h>
#include "device_state.h"
#include "lvgl_app_screen.h"

#ifndef USE_LVGL_UI
#define USE_LVGL_UI 0
#endif

#if USE_LVGL_UI
#include <lvgl.h>

class BuddyScreen final : public LvglAppScreen {
 public:
  void attach(lv_obj_t* parent, lv_group_t* group) override;
  void detach() override;
  void sync(const DeviceState& state) override;

 private:
  static String shortText(const String& value, size_t max_chars);
  static void setLabelText(lv_obj_t* obj, String& cache, const String& value);
  lv_obj_t* createRow(lv_obj_t* parent, int y, const char* label_text);

  lv_obj_t* root_ = nullptr;
  lv_group_t* group_ = nullptr;
  lv_obj_t* wifi_value_ = nullptr;
  lv_obj_t* bridge_value_ = nullptr;
  lv_obj_t* usage_value_ = nullptr;
  lv_obj_t* project_value_ = nullptr;
  lv_obj_t* branch_value_ = nullptr;
  lv_obj_t* thread_value_ = nullptr;
  String last_wifi_;
  String last_bridge_;
  String last_usage_;
  String last_project_;
  String last_branch_;
  String last_thread_;
};
#else
class BuddyScreen final : public LvglAppScreen {
 public:
  void attach(void*, void*) {}
  void detach() {}
  void sync(const DeviceState&) {}
};
#endif
