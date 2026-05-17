#pragma once

#include <Arduino.h>

#include "apps.h"
#include "device_state.h"
#include "middleware_link.h"
#include "network_manager.h"
#include "text_screen.h"

class AppShell {
 public:
  void begin();
  void handleCommand(const String& command);
  void tick();
  void render();
  void handleKeyboardInput(const String& typed, bool submit, bool backspace);
  void handlePushToTalk(bool pressed);
  bool hasPendingApproval() const;
  bool hasPendingBridgePrompt() const;
  void handleApprovalDecision(bool approved);
  void handleBridgePromptDecision(bool accepted);
  bool isPushToCodexActive() const;
  MiddlewareLink& bridge();

 private:
  void switchTo(AppId app_id);
  void traceDisplay();
  void emitDisplaySnapshot();

  DeviceState state_;
  NetworkManager network_;
  MiddlewareLink bridge_;
  TextScreen screen_;
  String input_line_;
  String last_display_trace_;
  BuddyApp buddy_app_;
  PushToCodexApp push_to_codex_app_;
  PagerApp pager_app_;
  McpBridgeApp mcp_bridge_app_;
  App* active_app_ = nullptr;
};
