#pragma once

#include <array>
#include <Arduino.h>

enum class AppId {
  Buddy,
  PushToCodex,
  Pager,
  Usage,
  McpBridge,
  Settings,
};

enum class UiMode {
  Home,
  Menu,
  Input,
  Modal,
  Approval,
  BridgePrompt,
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

enum class BridgePromptKind {
  None,
  Notification,
  Question,
  Confirmation,
};

enum class PagerScreen {
  Compose,
  Inbox,
  Detail,
};

struct MenuState {
  size_t active_tab = 0;
  size_t app_menu_selected = 0;
  size_t selected_index = 0;
  size_t scroll_offset = 0;
  bool app_menu_open = false;
  bool command_palette_open = false;
};

struct PagerSessionEvent {
  String type;
  String content;
};

struct PagerSessionSummary {
  static constexpr size_t kMaxEvents = 8;

  String session_id;
  String thread_id;
  String workspace_path;
  String branch;
  String title;
  String status;
  String last_event;
  String pending_approval_id;
  std::array<PagerSessionEvent, kMaxEvents> events{};
  size_t event_count = 0;
};

struct PagerViewState {
  static constexpr size_t kMaxSessions = 6;

  std::array<PagerSessionSummary, kMaxSessions> sessions{};
  size_t session_count = 0;
  String active_session_id;
  String selected_session_id;
  bool interrupt_supported = false;
};

struct DeviceState {
  static constexpr size_t kActivityLogSize = 8;

  String firmware_name;
  AppId active_app = AppId::Buddy;
  UiMode ui_mode = UiMode::Home;
  MenuState menu;
  CodexState codex_state = CodexState::Offline;
  PushToTalkState ptt_state = PushToTalkState::Idle;
  bool wifi_connected = false;
  int battery_percent = 0;
  int battery_voltage_mv = 0;
  int ptt_peak_amplitude = 0;
  int codex_usage_percent = -1;
  int codex_usage_secondary_percent = -1;
  uint32_t state_epoch = 0;
  int codex_usage_window_minutes = 0;
  int codex_usage_secondary_window_minutes = 0;
  uint32_t codex_usage_resets_at = 0;
  uint32_t codex_usage_secondary_resets_at = 0;
  String codex_usage_reset_line;
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
  BridgePromptKind bridge_prompt_kind = BridgePromptKind::None;
  bool bridge_prompt_pending = false;
  String bridge_prompt_title;
  String bridge_prompt_detail;
  std::array<String, 3> bridge_prompt_options{};
  size_t bridge_prompt_option_count = 0;
  size_t bridge_prompt_selected_index = 0;
  String wifi_ssid;
  String wifi_ip;
  String network_status_line;
  String status_line;
  String codex_usage_label;
  String codex_usage_detail_line;
  PagerViewState pager;
  PagerScreen pager_screen = PagerScreen::Inbox;
  size_t ptt_samples_captured = 0;
  size_t ptt_sample_limit = 0;
  uint32_t ptt_sample_rate_hz = 16000;
  String ptt_detail_line;
  unsigned long last_interaction_ms = 0;
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
