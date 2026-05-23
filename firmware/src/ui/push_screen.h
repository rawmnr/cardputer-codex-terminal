#pragma once

#include <Arduino.h>
#include "device_state.h"
#include "lvgl_app_screen.h"

#ifndef USE_LVGL_UI
#define USE_LVGL_UI 0
#endif

#if USE_LVGL_UI
#include <lvgl.h>
#include "ptt_widget.h"

class PushScreen final : public LvglAppScreen {
 public:
  void attach(lv_obj_t* parent, lv_group_t* group) override;
  void detach() override;
  void sync(const DeviceState& state) override;

 private:
  static void setLabelText(lv_obj_t* obj, String& cache, const String& value);

  lv_obj_t* root_ = nullptr;
  PttWidget ptt_;
  lv_obj_t* intent_label_ = nullptr;
  lv_obj_t* target_label_ = nullptr;
  lv_obj_t* status_ = nullptr;
  lv_obj_t* detail_ = nullptr;
  String last_intent_;
  String last_target_;
  String last_status_;
  String last_detail_;
};
#else
class PushScreen final : public LvglAppScreen {
 public:
  void attach(void*, void*) {}
  void detach() {}
  void sync(const DeviceState&) {}
};
#endif
