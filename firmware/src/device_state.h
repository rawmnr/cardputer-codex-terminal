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
  int battery_voltage_mv = 0;
  String wifi_ssid;
  String wifi_ip;
  String network_status_line;
  String status_line;
};
