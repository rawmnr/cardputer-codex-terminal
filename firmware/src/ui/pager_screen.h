#pragma once

#include <Arduino.h>
#include "device_state.h"
#include "lvgl_app_screen.h"

#ifndef USE_LVGL_UI
#define USE_LVGL_UI 0
#endif

#if USE_LVGL_UI
#include <lvgl.h>

class PagerAppScreen final : public LvglAppScreen {
 public:
  void attach(lv_obj_t* parent, lv_group_t* group) override;
  void detach() override;
  void sync(const DeviceState& state) override;

 private:
  static void setLabelText(lv_obj_t* obj, String& cache, const String& value);
  static String shortText(const String& value, size_t max_chars);
  static String summarizeEvent(const PagerSessionEvent& event);

  lv_obj_t* root_ = nullptr;
  lv_obj_t* mode_ = nullptr;
  lv_obj_t* title_ = nullptr;
  lv_obj_t* body_[4]{};
  String last_mode_;
  String last_title_;
  std::array<String, 4> last_body_{}; 
};
#else
class PagerAppScreen final : public LvglAppScreen {
 public:
  void attach(void*, void*) {}
  void detach() {}
  void sync(const DeviceState&) {}
};
#endif
