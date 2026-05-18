#pragma once

#include <Arduino.h>
#include "device_state.h"
#include "lvgl_app_screen.h"

#ifndef USE_LVGL_UI
#define USE_LVGL_UI 0
#endif

#if USE_LVGL_UI
#include <lvgl.h>

class SettingsScreen final : public LvglAppScreen {
 public:
  void attach(lv_obj_t* parent, lv_group_t* group) override;
  void detach() override;
  void sync(const DeviceState& state) override;

 private:
  static void setLabelText(lv_obj_t* obj, String& cache, const String& value);
  static void applySelectedStyle(lv_obj_t* row, lv_obj_t* label, bool selected);

  lv_obj_t* root_ = nullptr;
  lv_obj_t* rows_[4]{};
  lv_obj_t* labels_[4]{};
  lv_obj_t* detail_ = nullptr;
  String last_detail_;
  std::array<String, 4> last_labels_{};
};
#else
class SettingsScreen final : public LvglAppScreen {
 public:
  void attach(void*, void*) {}
  void detach() {}
  void sync(const DeviceState&) {}
};
#endif
