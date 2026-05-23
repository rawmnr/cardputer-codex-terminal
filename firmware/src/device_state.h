#pragma once

#include <array>
#include <Arduino.h>

enum class AppId {
  Buddy,
  PushToCodex,
  Runs,
  Approvals,
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

enum class RunsScreen {
  List,
  Detail,
  Actions,
  Diff,
  Tests,
};

enum class ApprovalsScreen {
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

struct RunSummaryView {
  String run_id;
  String title;
  String branch;
  String mode;
  String status;
  String last_event;
  String thread_id;
  String approval_id;
  String approval_title;
  String danger_level;
  bool merge_ready = false;
  bool worktree_exists = true;
};

struct RunDetailView {
  String run_id;
  String role;
  String title;
  String thread_id;
  String session_id;
  String workspace_path;
  String branch;
  String mode;
  String status;
  String step;
  String last_event;
  String diff_summary;
  String test_summary;
  String danger;
  String badge;
  String stale;
  String approval_id;
  String approval_title;
  String approval_detail;
  String approval_danger;
  bool approval_pending = false;
  bool merge_ready = false;
  bool worktree_exists = true;
  int diff_files = 0;
  int diff_insertions = 0;
  int diff_deletions = 0;
  int test_run = 0;
  int test_passed = 0;
  int test_failed = 0;
  int test_skipped = 0;
};

struct ApprovalInboxItem {
  String run_id;
  String run_title;
  String approval_id;
  String title;
  String detail;
  String danger_level;
  String mode;
  String status;
  String branch;
  String thread_id;
};

struct PagerViewState {
  static constexpr size_t kMaxSessions = 6;

  std::array<PagerSessionSummary, kMaxSessions> sessions{};
  size_t session_count = 0;
  String active_session_id;
  String selected_session_id;
  bool interrupt_supported = false;
};

struct RunsViewState {
  static constexpr size_t kMaxRuns = 6;

  std::array<RunSummaryView, kMaxRuns> runs{};
  size_t run_count = 0;
  String active_run_id;
  String selected_run_id;
  size_t selected_index = 0;
  size_t scroll_offset = 0;
  RunsScreen screen = RunsScreen::List;
  RunDetailView detail;
  std::array<String, 6> actions{};
  size_t action_count = 0;
  size_t selected_action_index = 0;
};

struct ApprovalInboxState {
  static constexpr size_t kMaxApprovals = 6;

  std::array<ApprovalInboxItem, kMaxApprovals> approvals{};
  size_t approval_count = 0;
  String active_run_id;
  String selected_approval_id;
  size_t selected_index = 0;
  size_t scroll_offset = 0;
  ApprovalsScreen screen = ApprovalsScreen::Inbox;
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
  bool ble_enabled = false;
  bool ble_advertising = false;
  bool ble_connected = false;
  String ble_name = "CardputerCodex";
  String ble_status_line = "BLE disabled";
  String active_transport = "wifi";
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
  RunsViewState runs;
  ApprovalInboxState approvals;
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
