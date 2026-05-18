#include "buddy_screen.h"

#if USE_LVGL_UI
namespace {
constexpr int kContentWidth = 224;
constexpr int kContentHeight = 84;
constexpr int kRowHeight = 13;
constexpr int kLabelWidth = 58;
constexpr int kValueX = 68;

void setRowStyle(lv_obj_t* obj) {
  lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}
}  // namespace

String BuddyScreen::shortText(const String& value, size_t max_chars) {
  if (value.length() <= max_chars) {
    return value;
  }
  if (max_chars <= 1) {
    return value.substring(0, max_chars);
  }
  return value.substring(0, max_chars - 1) + "~";
}

void BuddyScreen::setLabelText(lv_obj_t* obj, String& cache, const String& value) {
  if (obj == nullptr || cache == value) {
    return;
  }

  cache = value;
  lv_label_set_text(obj, value.c_str());
}

lv_obj_t* BuddyScreen::createRow(lv_obj_t* parent, int y, const char* label_text) {
  lv_obj_t* row = lv_obj_create(parent);
  lv_obj_set_size(row, kContentWidth, kRowHeight);
  lv_obj_align(row, LV_ALIGN_TOP_LEFT, 0, y);
  setRowStyle(row);

  lv_obj_t* key = lv_label_create(row);
  lv_obj_set_width(key, kLabelWidth);
  lv_obj_set_style_text_color(key, lv_color_hex(0x7FE7FF), 0);
  lv_obj_set_style_text_font(key, &lv_font_montserrat_14, 0);
  lv_label_set_long_mode(key, LV_LABEL_LONG_CLIP);
  lv_label_set_text(key, label_text);

  lv_obj_t* value = lv_label_create(row);
  lv_obj_set_width(value, kContentWidth - kValueX);
  lv_obj_align(value, LV_ALIGN_TOP_LEFT, kValueX, 0);
  lv_obj_set_style_text_color(value, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(value, &lv_font_montserrat_14, 0);
  lv_label_set_long_mode(value, LV_LABEL_LONG_CLIP);
  lv_label_set_text(value, "");

  return value;
}

void BuddyScreen::attach(lv_obj_t* parent, lv_group_t* group) {
  group_ = group;
  root_ = lv_obj_create(parent);
  lv_obj_remove_style_all(root_);
  lv_obj_set_size(root_, kContentWidth, kContentHeight);
  lv_obj_align(root_, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(root_, 0, 0);
  lv_obj_set_style_pad_all(root_, 0, 0);
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

  wifi_value_ = createRow(root_, 0, "Wi-Fi");
  bridge_value_ = createRow(root_, 14, "Bridge");
  usage_value_ = createRow(root_, 28, "Usage");
  project_value_ = createRow(root_, 42, "Project");
  branch_value_ = createRow(root_, 56, "Branch");
  thread_value_ = createRow(root_, 70, "Thread");

  sync(DeviceState{});
}

void BuddyScreen::detach() {
  if (root_ != nullptr) {
    lv_obj_del(root_);
    root_ = nullptr;
  }
  group_ = nullptr;
  wifi_value_ = nullptr;
  bridge_value_ = nullptr;
  usage_value_ = nullptr;
  project_value_ = nullptr;
  branch_value_ = nullptr;
  thread_value_ = nullptr;
  last_wifi_ = "";
  last_bridge_ = "";
  last_usage_ = "";
  last_project_ = "";
  last_branch_ = "";
  last_thread_ = "";
}

void BuddyScreen::sync(const DeviceState& state) {
  if (root_ == nullptr) {
    return;
  }

  String wifi = state.wifi_connected ? String("ON") : String("OFF");
  if (state.wifi_ssid.length() > 0) {
    wifi += " ";
    wifi += state.wifi_ssid;
  }
  if (state.wifi_ip.length() > 0) {
    wifi += " ";
    wifi += state.wifi_ip;
  }
  setLabelText(wifi_value_, last_wifi_, shortText(wifi, 24));

  String bridge = state.bridge_status_line.length() > 0 ? state.bridge_status_line : String("idle");
  if (state.network_status_line.length() > 0) {
    bridge += " | ";
    bridge += state.network_status_line;
  }
  setLabelText(bridge_value_, last_bridge_, shortText(bridge, 24));

  String usage = state.codex_usage_percent >= 0 ? String(state.codex_usage_percent) + "%" : String("--");
  if (state.codex_usage_window_minutes > 0) {
    usage += " / ";
    usage += state.codex_usage_window_minutes;
    usage += "m";
  }
  if (state.codex_usage_reset_line.length() > 0) {
    usage += " ";
    usage += state.codex_usage_reset_line;
  }
  setLabelText(usage_value_, last_usage_, shortText(usage, 24));

  String project = state.codex_workspace_path.length() > 0 ? state.codex_workspace_path : String("(none)");
  setLabelText(project_value_, last_project_, shortText(project, 24));

  String branch = state.codex_branch.length() > 0 ? state.codex_branch : String("-");
  setLabelText(branch_value_, last_branch_, shortText(branch, 24));

  String thread = state.codex_thread_id.length() > 0 ? state.codex_thread_id : String("-");
  if (state.codex_state == CodexState::WaitingForApproval && state.approval_pending) {
    thread += " approval";
  }
  setLabelText(thread_value_, last_thread_, shortText(thread, 24));
}
#endif
