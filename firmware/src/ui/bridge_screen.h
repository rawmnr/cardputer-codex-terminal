#pragma once

#include <Arduino.h>
#include "device_state.h"
#include "lvgl_app_screen.h"

#ifndef USE_LVGL_UI
#define USE_LVGL_UI 0
#endif

#if USE_LVGL_UI
#include <lvgl.h>

class BridgeScreen final : public LvglAppScreen {
 public:
  void attach(lv_obj_t* parent, lv_group_t* group) override;
  void detach() override;
  void sync(const DeviceState& state) override;

 private:
  static void setLabelText(lv_obj_t* obj, String& cache, const String& value);

  lv_obj_t* root_ = nullptr;
  lv_obj_t* status_ = nullptr;
  lv_obj_t* title_ = nullptr;
  lv_obj_t* detail_ = nullptr;
  lv_obj_t* options_[3]{};
  String last_status_;
  String last_title_;
  String last_detail_;
  std::array<String, 3> last_options_{};
};
#else
class BridgeScreen final : public LvglAppScreen {
 public:
  void attach(void*, void*) {}
  void detach() {}
  void sync(const DeviceState&) {}
};
#endif
