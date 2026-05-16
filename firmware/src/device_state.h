#pragma once

#include <Arduino.h>

enum class AppId {
  Buddy,
  PushToCodex,
  Pager,
  McpBridge,
};

enum class CodexState {
  Offline,
  Idle,
  Busy,
  WaitingForApproval,
};

struct DeviceState {
  String firmware_name;
  AppId active_app = AppId::Buddy;
  CodexState codex_state = CodexState::Offline;
  bool wifi_connected = false;
  int battery_percent = 0;
  String status_line;
};

