#pragma once

#include <array>
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
  static constexpr size_t kActivityLogSize = 8;

  String firmware_name;
  AppId active_app = AppId::Buddy;
  CodexState codex_state = CodexState::Offline;
  PushToTalkState ptt_state = PushToTalkState::Idle;
  bool wifi_connected = false;
  int battery_percent = 0;
  int battery_voltage_mv = 0;
  int ptt_peak_amplitude = 0;
  int codex_usage_percent = -1;
  int codex_usage_window_minutes = 0;
  uint32_t codex_usage_resets_at = 0;
  String codex_workspace_path = ".";
  String codex_branch;
  String codex_thread_id;
  String approval_id;
  String approval_title;
  String approval_detail_line;
  int approval_timeout_seconds = 0;
  bool approval_pending = false;
  String codex_stream_line;
  String bridge_status_line;
  String wifi_ssid;
  String wifi_ip;
  String network_status_line;
  String status_line;
  String codex_usage_label;
  String codex_usage_detail_line;
  size_t ptt_samples_captured = 0;
  size_t ptt_sample_limit = 0;
  uint32_t ptt_sample_rate_hz = 16000;
  String ptt_detail_line;
  std::array<String, kActivityLogSize> activity_log{};
  size_t activity_log_head = 0;
  size_t activity_log_count = 0;
};

inline void append_activity_event(DeviceState& state, const String& message) {
  state.activity_log[state.activity_log_head] = message;
  state.activity_log_head = (state.activity_log_head + 1) % DeviceState::kActivityLogSize;
  if (state.activity_log_count < DeviceState::kActivityLogSize) {
    state.activity_log_count++;
  }
}

inline String activity_log_entry(const DeviceState& state, size_t index) {
  if (index >= state.activity_log_count) {
    return "";
  }

  const size_t start = (state.activity_log_head + DeviceState::kActivityLogSize - state.activity_log_count) % DeviceState::kActivityLogSize;
  const size_t actual = (start + index) % DeviceState::kActivityLogSize;
  return state.activity_log[actual];
}
