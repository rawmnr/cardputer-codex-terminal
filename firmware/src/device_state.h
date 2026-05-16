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

enum class PushToTalkState {
  Idle,
  Armed,
  Recording,
  Ready,
  Error,
};

struct DeviceState {
  String firmware_name;
  AppId active_app = AppId::Buddy;
  CodexState codex_state = CodexState::Offline;
  PushToTalkState ptt_state = PushToTalkState::Idle;
  bool wifi_connected = false;
  int battery_percent = 0;
  int battery_voltage_mv = 0;
  int ptt_peak_amplitude = 0;
  String wifi_ssid;
  String wifi_ip;
  String network_status_line;
  String status_line;
  size_t ptt_samples_captured = 0;
  size_t ptt_sample_limit = 0;
  uint32_t ptt_sample_rate_hz = 16000;
  String ptt_detail_line;
};
