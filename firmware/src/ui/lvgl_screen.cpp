#include "lvgl_screen.h"

#if USE_LVGL_UI
namespace {
constexpr size_t kTabCount = 6;

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

void LvglScreen::setLabelText(lv_obj_t* obj, String& cache, const String& value) {
  if (obj == nullptr || cache == value) {
    return;
  }

  cache = value;
  lv_label_set_text(obj, value.c_str());
}

size_t LvglScreen::tabIndexForApp(AppId app_id) {
  switch (app_id) {
    case AppId::Buddy:
      return 0;
    case AppId::PushToCodex:
      return 1;
    case AppId::Pager:
      return 2;
    case AppId::Usage:
      return 3;
    case AppId::McpBridge:
      return 4;
    case AppId::Settings:
      return 5;
  }
  return 0;
}

const char* LvglScreen::tabLabel(size_t index) {
  if (index >= kTabCount) {
    return "App";
  }
  return kTabMap[index];
}

void LvglScreen::begin() {
  port_.begin();

  root_ = lv_screen_active();
  lv_obj_set_style_bg_color(root_, lv_color_hex(0x071521), 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

  title_ = lv_label_create(root_);
  lv_obj_set_style_text_color(title_, lv_color_hex(0x7FE7FF), 0);
  lv_obj_align(title_, LV_ALIGN_TOP_LEFT, 8, 4);
  setLabelText(title_, last_title_, "Cardputer Codex");

  active_app_ = lv_label_create(root_);
  lv_obj_set_style_text_color(active_app_, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(active_app_, LV_ALIGN_TOP_LEFT, 8, 20);
  setLabelText(active_app_, last_active_app_, "Codex Buddy");

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
  setLabelText(status_, last_status_, "Ready");

  detail_ = lv_label_create(root_);
  lv_obj_set_width(detail_, 224);
  lv_obj_set_style_text_color(detail_, lv_color_hex(0x9FB0BF), 0);
  lv_obj_align(detail_, LV_ALIGN_TOP_LEFT, 8, 58);
  lv_label_set_long_mode(detail_, LV_LABEL_LONG_CLIP);
  setLabelText(detail_, last_detail_, "Mode Home | Offline");

  tabs_ = lv_buttonmatrix_create(root_);
  lv_obj_set_size(tabs_, 224, 42);
  lv_obj_align(tabs_, LV_ALIGN_BOTTOM_MID, 0, -18);
  lv_buttonmatrix_set_map(tabs_, kTabMap);
  lv_buttonmatrix_set_one_checked(tabs_, true);
  lv_obj_add_event_cb(tabs_, onTabEvent, LV_EVENT_VALUE_CHANGED, this);
  lv_group_add_obj(port_.group(), tabs_);
  lv_obj_add_flag(tabs_, LV_OBJ_FLAG_HIDDEN);

  focus_ = lv_label_create(root_);
  lv_obj_set_width(focus_, 224);
  lv_obj_set_style_text_color(focus_, lv_color_hex(0x7FE7FF), 0);
  lv_obj_align(focus_, LV_ALIGN_BOTTOM_LEFT, 8, -18);
  lv_label_set_long_mode(focus_, LV_LABEL_LONG_CLIP);
  setLabelText(focus_, last_focus_, "Focus: Buddy");

  footer_ = lv_label_create(root_);
  lv_obj_set_width(footer_, 224);
  lv_obj_set_style_text_color(footer_, lv_color_hex(0x9FB0BF), 0);
  lv_obj_align(footer_, LV_ALIGN_BOTTOM_LEFT, 8, -2);
  lv_label_set_long_mode(footer_, LV_LABEL_LONG_CLIP);
  setLabelText(footer_, last_footer_, "");

  lv_buttonmatrix_set_selected_button(tabs_, 0);
  last_tab_selection_ = 0;
  last_menu_open_ = false;
  lv_scr_load(root_);
}

void LvglScreen::syncMenuState(const DeviceState& state) {
  if (tabs_ == nullptr) {
    return;
  }

  const bool menu_open = state.menu.app_menu_open;
  if (menu_open != last_menu_open_) {
    if (menu_open) {
      lv_obj_clear_flag(tabs_, LV_OBJ_FLAG_HIDDEN);
      lv_group_focus_obj(tabs_);
    } else {
      lv_obj_add_flag(tabs_, LV_OBJ_FLAG_HIDDEN);
    }
    last_menu_open_ = menu_open;
  } else if (menu_open) {
    lv_group_focus_obj(tabs_);
  }

  const size_t desired_selection = menu_open ? state.menu.app_menu_selected : tabIndexForApp(state.active_app);
  if (desired_selection != last_tab_selection_) {
    lv_buttonmatrix_set_selected_button(tabs_, desired_selection);
    last_tab_selection_ = desired_selection;
  }
}

void LvglScreen::renderShell(const DeviceState& state, App& app, const String& input_line, const String& footer_hint) {
  if (root_ == nullptr) {
    begin();
  }

  syncMenuState(state);

  const size_t active_index = tabIndexForApp(state.active_app);
  const size_t focus_index = state.menu.app_menu_open ? state.menu.app_menu_selected : active_index;

  const String title = state.menu.app_menu_open ? "Applications" : "Cardputer Codex";
  setLabelText(title_, last_title_, title);

  String active = state.menu.app_menu_open ? String("Current: ") + active_app_label(state.active_app)
                                           : active_app_label(state.active_app);
  setLabelText(active_app_, last_active_app_, active);

  String status = state.status_line.length() > 0 ? state.status_line : String("Ready");
  if (state.menu.command_palette_open && input_line.length() > 0) {
    status = String("Cmd: ") + input_line;
  }
  setLabelText(status_, last_status_, short_status(status, 48));

  String detail = String("Mode ") + ui_mode_label(state.ui_mode);
  detail += " | ";
  detail += codex_label(state.codex_state);
  if (state.bridge_status_line.length() > 0) {
    detail += " | ";
    detail += state.bridge_status_line;
  }
  setLabelText(detail_, last_detail_, short_status(detail, 56));

  String wifi = state.wifi_connected ? String("Wi-Fi ON") : String("Wi-Fi OFF");
  if (state.wifi_ssid.length() > 0) {
    wifi += " ";
    wifi += state.wifi_ssid;
  }
  setLabelText(wifi_, last_wifi_, short_status(wifi, 20));

  String codex = String("Codex ") + codex_label(state.codex_state);
  if (state.codex_usage_percent >= 0) {
    codex += " ";
    codex += state.codex_usage_percent;
    codex += "%";
  }
  setLabelText(codex_, last_codex_, short_status(codex, 20));

  String battery = String("Battery ");
  battery += state.battery_percent;
  battery += "%";
  setLabelText(battery_, last_battery_, short_status(battery, 20));

  String footer = short_status(footer_hint, 48);
  setLabelText(footer_, last_footer_, footer);

  if (state.menu.command_palette_open) {
    setLabelText(focus_, last_focus_, "Focus: command palette");
  } else if (state.menu.app_menu_open) {
    const char* focus_text = tabLabel(focus_index);
    String label = String("Focus: ") + focus_text;
    setLabelText(focus_, last_focus_, label);
  } else {
    const String label = String("Focus: ") + app.title();
    setLabelText(focus_, last_focus_, label);
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
    setLabelText(self->focus_, self->last_focus_, "Focus: none");
    return;
  }

  String label = String("Focus: ") + text;
  setLabelText(self->focus_, self->last_focus_, label);
}
#endif
