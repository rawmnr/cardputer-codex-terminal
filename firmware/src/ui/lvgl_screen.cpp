#include "lvgl_screen.h"

#if USE_LVGL_UI
namespace {
const char* codex_label(CodexState state) {
  switch (state) {
    case CodexState::Offline:
      return "Offline";
    case CodexState::Idle:
      return "Idle";
    case CodexState::Busy:
      return "Busy";
    case CodexState::WaitingForApproval:
      return "Approval";
  }
  return "Unknown";
}

const char* ui_mode_label(UiMode mode) {
  switch (mode) {
    case UiMode::Home:
      return "Home";
    case UiMode::Menu:
      return "Menu";
    case UiMode::Input:
      return "Input";
    case UiMode::Modal:
      return "Modal";
    case UiMode::Approval:
      return "Approval";
    case UiMode::BridgePrompt:
      return "Bridge";
  }
  return "Mode";
}

const char* active_app_label(AppId id) {
  switch (id) {
    case AppId::Buddy:
      return "Codex Buddy";
    case AppId::PushToCodex:
      return "Push to Codex";
    case AppId::Pager:
      return "Codex Pager";
    case AppId::Usage:
      return "Codex Usage";
    case AppId::McpBridge:
      return "MCP Bridge";
    case AppId::Settings:
      return "Settings";
  }
  return "App";
}

String short_status(const String& value, size_t max_chars) {
  if (value.length() <= max_chars) {
    return value;
  }
  if (max_chars <= 1) {
    return value.substring(0, max_chars);
  }
  return value.substring(0, max_chars - 1) + "~";
}
}  // namespace

const char* const LvglScreen::kTabMap[] = {
  "Buddy", "Push", "Pager", "Usage", "MCP", "Set", nullptr,
};

void LvglScreen::begin() {
  port_.begin();

  root_ = lv_screen_active();
  lv_obj_set_style_bg_color(root_, lv_color_hex(0x071521), 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

  title_ = lv_label_create(root_);
  lv_obj_set_style_text_color(title_, lv_color_hex(0x7FE7FF), 0);
  lv_obj_align(title_, LV_ALIGN_TOP_LEFT, 8, 4);
  lv_label_set_text(title_, "Cardputer Codex");

  active_app_ = lv_label_create(root_);
  lv_obj_set_style_text_color(active_app_, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(active_app_, LV_ALIGN_TOP_LEFT, 8, 20);
  lv_label_set_text(active_app_, "Codex Buddy");

  wifi_ = lv_label_create(root_);
  lv_obj_align(wifi_, LV_ALIGN_TOP_RIGHT, -8, 6);

  codex_ = lv_label_create(root_);
  lv_obj_align(codex_, LV_ALIGN_TOP_RIGHT, -8, 20);

  battery_ = lv_label_create(root_);
  lv_obj_align(battery_, LV_ALIGN_TOP_RIGHT, -8, 34);

  status_ = lv_label_create(root_);
  lv_obj_set_width(status_, 224);
  lv_obj_set_style_text_color(status_, lv_color_hex(0xD7E0EA), 0);
  lv_obj_align(status_, LV_ALIGN_TOP_LEFT, 8, 42);
  lv_label_set_long_mode(status_, LV_LABEL_LONG_CLIP);

  detail_ = lv_label_create(root_);
  lv_obj_set_width(detail_, 224);
  lv_obj_set_style_text_color(detail_, lv_color_hex(0x9FB0BF), 0);
  lv_obj_align(detail_, LV_ALIGN_TOP_LEFT, 8, 58);
  lv_label_set_long_mode(detail_, LV_LABEL_LONG_CLIP);

  tabs_ = lv_buttonmatrix_create(root_);
  lv_obj_set_size(tabs_, 224, 42);
  lv_obj_align(tabs_, LV_ALIGN_BOTTOM_MID, 0, -18);
  lv_buttonmatrix_set_map(tabs_, kTabMap);
  lv_obj_add_event_cb(tabs_, onTabEvent, LV_EVENT_VALUE_CHANGED, this);
  lv_group_add_obj(port_.group(), tabs_);

  focus_ = lv_label_create(root_);
  lv_obj_set_width(focus_, 224);
  lv_obj_set_style_text_color(focus_, lv_color_hex(0x7FE7FF), 0);
  lv_obj_align(focus_, LV_ALIGN_BOTTOM_LEFT, 8, -4);
  lv_label_set_long_mode(focus_, LV_LABEL_LONG_CLIP);
  lv_label_set_text(focus_, "Focus: Buddy");

  footer_ = lv_label_create(root_);
  lv_obj_set_width(footer_, 224);
  lv_obj_set_style_text_color(footer_, lv_color_hex(0x9FB0BF), 0);
  lv_obj_align(footer_, LV_ALIGN_BOTTOM_LEFT, 8, -2);
  lv_label_set_long_mode(footer_, LV_LABEL_LONG_CLIP);

  lv_buttonmatrix_set_selected_button(tabs_, 0);
  lv_scr_load(root_);
}

void LvglScreen::renderShell(const DeviceState& state, App& app, const String& input_line, const String& footer_hint) {
  if (root_ == nullptr) {
    begin();
  }

  lv_label_set_text(active_app_, active_app_label(state.active_app));

  String status = state.status_line.length() > 0 ? state.status_line : String("Ready");
  if (state.menu.command_palette_open && input_line.length() > 0) {
    status = String("Cmd: ") + input_line;
  }
  lv_label_set_text(status_, short_status(status, 48).c_str());

  String detail = String("Mode ") + ui_mode_label(state.ui_mode);
  detail += " | ";
  detail += codex_label(state.codex_state);
  if (state.bridge_status_line.length() > 0) {
    detail += " | ";
    detail += state.bridge_status_line;
  }
  lv_label_set_text(detail_, short_status(detail, 56).c_str());

  String wifi = state.wifi_connected ? String("Wi-Fi ON") : String("Wi-Fi OFF");
  if (state.wifi_ssid.length() > 0) {
    wifi += " ";
    wifi += state.wifi_ssid;
  }
  lv_label_set_text(wifi_, short_status(wifi, 20).c_str());

  String codex = String("Codex ") + codex_label(state.codex_state);
  if (state.codex_usage_percent >= 0) {
    codex += " ";
    codex += state.codex_usage_percent;
    codex += "%";
  }
  lv_label_set_text(codex_, short_status(codex, 20).c_str());

  String battery = String("Battery ");
  battery += state.battery_percent;
  battery += "%";
  lv_label_set_text(battery_, short_status(battery, 20).c_str());

  String footer = short_status(footer_hint, 48);
  lv_label_set_text(footer_, footer.c_str());

  if (state.menu.command_palette_open) {
    lv_label_set_text(focus_, "Focus: command palette");
  } else if (state.menu.app_menu_open) {
    lv_label_set_text(focus_, "Focus: app menu");
  } else {
    const String label = String("Focus: ") + app.title();
    lv_label_set_text(focus_, label.c_str());
  }
}

void LvglScreen::pushKey(lv_key_t key, bool pressed) {
  port_.pushKey(key, pressed);
}

void LvglScreen::tick() {
  port_.tick();
}

void LvglScreen::onTabEvent(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) {
    return;
  }

  auto* obj = static_cast<lv_obj_t*>(lv_event_get_target(event));
  auto* self = static_cast<LvglScreen*>(lv_event_get_user_data(event));
  if (obj == nullptr || self == nullptr) {
    return;
  }

  const uint32_t index = lv_buttonmatrix_get_selected_button(obj);
  const char* text = index == LV_BUTTONMATRIX_BUTTON_NONE ? nullptr : lv_buttonmatrix_get_button_text(obj, index);
  if (text == nullptr) {
    lv_label_set_text(self->focus_, "Focus: none");
    return;
  }

  String label = String("Focus: ") + text;
  lv_label_set_text(self->focus_, label.c_str());
}
#endif
