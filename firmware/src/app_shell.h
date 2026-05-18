#pragma once

#include <Arduino.h>

#include "apps.h"
#include "device_state.h"
#include "middleware_link.h"
#include "network_manager.h"
#include "text_screen.h"

#ifndef USE_LVGL_UI
#define USE_LVGL_UI 0
#endif

#if USE_LVGL_UI
#include "ui/lvgl_screen.h"
#endif

class AppShell {
 public:
  void begin();
  void handleCommand(const String& command);
  void tick();
  void render();
  void handleTextInput(const String& typed, bool submit, bool backspace);
  void handleAction(UiAction action);
  void handlePushToTalk(bool pressed);
  bool hasPendingApproval() const;
  bool hasPendingBridgePrompt() const;
  void handleApprovalDecision(bool approved);
  void handleBridgePromptDecision(bool accepted);
#if USE_LVGL_UI
  void handleUiKey(lv_key_t key, bool pressed);
#endif
  void noteInteraction();
  bool isPushToCodexActive() const;
  bool isAppMenuOpen() const;
  bool hasVisibleModal() const;
  UiMode uiMode() const;
  MiddlewareLink& bridge();

 private:
  enum class DisplayPowerState {
    Active,
    Dimmed,
    LowPower,
  };

  void switchTo(AppId app_id);
  void setActiveTab(size_t tab_index);
  size_t tabIndexForApp(AppId app_id) const;
  AppId appForTab(size_t tab_index) const;
  String footerHint() const;
  void traceDisplay();
  void emitDisplaySnapshot();
  uint8_t brightnessForIdle(unsigned long idle_ms) const;
  void applyDisplayBrightness(uint8_t brightness, DisplayPowerState state);

  DeviceState state_;
  NetworkManager network_;
  MiddlewareLink bridge_;
#if USE_LVGL_UI
  LvglScreen lvgl_screen_;
#endif
  TextScreen screen_;
  String input_line_;
  String last_display_trace_;
  BuddyApp buddy_app_;
  PushToCodexApp push_to_codex_app_;
  PagerApp pager_app_;
  UsageApp usage_app_;
  McpBridgeApp mcp_bridge_app_;
  SettingsApp settings_app_;
  App* active_app_ = nullptr;
  DisplayPowerState display_power_state_ = DisplayPowerState::LowPower;
  uint8_t display_brightness_ = 0;
};
