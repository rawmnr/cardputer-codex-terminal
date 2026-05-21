#pragma once

#include <Arduino.h>
#include <iostream>

#ifndef USE_LVGL_UI
#define USE_LVGL_UI 0
#endif

#if USE_LVGL_UI
#include <lvgl.h>

#include "apps.h"
#include "device_state.h"
#include "buddy_screen.h"
#include "bridge_screen.h"
#include "runs_screen.h"
#include "approvals_screen.h"
#include "pager_screen.h"
#include "push_screen.h"
#include "settings_screen.h"
#include "usage_screen.h"
#include "ptt_widget.h"
#include "modal.h"
#include "lvgl_app_screen.h"
#include "lvgl_port.h"

class LvglScreen {
 public:
  void begin();
  void renderShell(const DeviceState& state, App& app, const String& input_line, const String& footer_hint);
  void pushKey(uint32_t key, bool pressed);
  void tick();
  const uint16_t* framebuffer() const;

 private:
  static constexpr size_t kNoSelection = static_cast<size_t>(-1);
  static const char* const kTabMap[];

  static void setLabelText(lv_obj_t* obj, String& cache, const String& value);
  static size_t tabIndexForApp(AppId app_id);
  static const char* tabLabel(size_t index);
  static void onTabEvent(lv_event_t* event);

  void syncMenuState(const DeviceState& state);
  void syncModalState(const DeviceState& state);
  void syncPttState(const DeviceState& state);
  void syncContentScreen(const DeviceState& state);

  LvglPort port_;
  ModalWidget modal_;
  PttWidget ptt_;
  BuddyScreen buddy_screen_;
  PushScreen push_screen_;
  RunsDashboardScreen runs_screen_;
  ApprovalsInboxScreen approvals_screen_;
  PagerAppScreen pager_screen_;
  UsageScreen usage_screen_;
  BridgeScreen bridge_screen_;
  SettingsScreen settings_screen_;
  LvglAppScreen* active_screen_ = nullptr;
  lv_obj_t* root_ = nullptr;
  lv_obj_t* title_ = nullptr;
  lv_obj_t* active_app_ = nullptr;
  lv_obj_t* content_root_ = nullptr;
  lv_obj_t* status_ = nullptr;
  lv_obj_t* detail_ = nullptr;
  lv_obj_t* wifi_ = nullptr;
  lv_obj_t* codex_ = nullptr;
  lv_obj_t* battery_ = nullptr;
  lv_obj_t* focus_ = nullptr;
  lv_obj_t* footer_ = nullptr;
  lv_obj_t* tabs_ = nullptr;
  String last_title_;
  String last_active_app_;
  String last_status_;
  String last_detail_;
  String last_wifi_;
  String last_codex_;
  String last_battery_;
  String last_focus_;
  String last_footer_;
  size_t last_tab_selection_ = kNoSelection;
  bool last_menu_open_ = false;
  bool last_content_open_ = false;
  AppId last_content_app_ = AppId::Buddy;
};
#else
#include "apps.h"
#include "device_state.h"
#include "lvgl_port.h"

class LvglScreen {
 public:
  void begin() {}
  void renderShell(const DeviceState&, App&, const String&, const String&) {}
  void pushKey(uint32_t, bool) {}
  void tick() {}
  const uint16_t* framebuffer() const { return nullptr; }
};
#endif
