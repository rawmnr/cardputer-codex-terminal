#pragma once

#include <Arduino.h>

#include "apps.h"
#include "device_state.h"
#include "text_screen.h"

class AppShell {
 public:
  void begin();
  void handleCommand(const String& command);
  void tick();
  void render();

 private:
  void switchTo(AppId app_id);
  void updateStatusFromCommand(const String& command);

  DeviceState state_;
  TextScreen screen_;
  BuddyApp buddy_app_;
  PushToCodexApp push_to_codex_app_;
  PagerApp pager_app_;
  McpBridgeApp mcp_bridge_app_;
  App* active_app_ = nullptr;
};

