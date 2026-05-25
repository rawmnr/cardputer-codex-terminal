#include "lvgl_screen.h"

#if USE_LVGL_UI
namespace {
constexpr size_t kTabCount = 8;

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
    case AppId::Runs:
      return "Runs";
    case AppId::Approvals:
      return "Approvals";
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

LvglAppScreen* screenForApp(
    AppId app_id,
    BuddyScreen& buddy_screen,
    PushScreen& push_screen,
    RunsDashboardScreen& runs_screen,
    ApprovalsInboxScreen& approvals_screen,
    PagerAppScreen& pager_screen,
    UsageScreen& usage_screen,
    BridgeScreen& bridge_screen,
    SettingsScreen& settings_screen) {
  switch (app_id) {
    case AppId::Buddy:
      return &buddy_screen;
    case AppId::PushToCodex:
      return &push_screen;
    case AppId::Runs:
      return &runs_screen;
    case AppId::Approvals:
      return &approvals_screen;
    case AppId::Pager:
      return &pager_screen;
    case AppId::Usage:
      return &usage_screen;
    case AppId::McpBridge:
      return &bridge_screen;
    case AppId::Settings:
      return &settings_screen;
  }
  return &buddy_screen;
}
}  // namespace

const char* const LvglScreen::kTabMap[] = {
  "Codex Buddy", "\n", "Push to Codex", "\n", "Runs", "\n", "Approvals", "\n", "Codex Pager", "\n", "Codex Usage", "\n", "MCP Bridge", "\n", "Settings", nullptr,
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
    case AppId::Runs:
      return 2;
    case AppId::Approvals:
      return 3;
    case AppId::Pager:
      return 4;
    case AppId::Usage:
      return 5;
    case AppId::McpBridge:
      return 6;
    case AppId::Settings:
      return 7;
  }
  return 0;
}

const char* LvglScreen::tabLabel(size_t index) {
  if (index >= kTabCount) {
    return "App";
  }
  return kTabMap[index * 2];
}

#if !defined(ARDUINO) || defined(NATIVE_BUILD)
const uint16_t* LvglScreen::framebuffer() const {
  return port_.framebuffer();
}
#else
const uint16_t* LvglScreen::framebuffer() const {
  return nullptr;
}
#endif

void LvglScreen::begin() {
  port_.begin();
  navigation_ = {};
  if (!port_.ready()) {
    return;
  }

  // Wait for port to be ready
  for (int i = 0; i < 10 && !port_.ready(); ++i) {
    port_.tick();
  }

  if (!port_.ready()) {
    return;
  }

  root_ = lv_screen_active();
  if (root_ == nullptr) {
    return;
  }
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
  lv_obj_set_size(tabs_, 224, 110);
  lv_obj_align(tabs_, LV_ALIGN_TOP_MID, 0, 18);
  lv_buttonmatrix_set_map(tabs_, kTabMap);
  lv_buttonmatrix_set_one_checked(tabs_, true);
  lv_obj_add_event_cb(tabs_, onTabEvent, LV_EVENT_VALUE_CHANGED, this);
  lv_group_add_obj(port_.group(), tabs_);
  lv_obj_add_flag(tabs_, LV_OBJ_FLAG_HIDDEN);

  modal_.begin(root_, port_.group());
  ptt_.begin(root_);

  content_root_ = lv_obj_create(root_);
  lv_obj_remove_style_all(content_root_);
  lv_obj_set_size(content_root_, 224, 84);
  lv_obj_align(content_root_, LV_ALIGN_TOP_LEFT, 8, 38);
  lv_obj_set_style_bg_opa(content_root_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(content_root_, 0, 0);
  lv_obj_set_style_pad_all(content_root_, 0, 0);
  lv_obj_clear_flag(content_root_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(content_root_, LV_OBJ_FLAG_HIDDEN);

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
  last_content_open_ = false;
  lv_scr_load(root_);
}

void LvglScreen::syncMenuState(const DeviceState& state) {
  if (tabs_ == nullptr) {
    return;
  }

  const bool modal_open = navigation_.modal_open;
  const bool menu_open = navigation_.menu_open && !modal_open;
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

void LvglScreen::syncModalState(const DeviceState& state) {
  const bool approval_open = navigation_.overlay == OverlayKind::Approval;
  const bool bridge_modal_open = navigation_.overlay == OverlayKind::BridgePrompt;

  if (!approval_open && !bridge_modal_open) {
    modal_.setVisible(false);
    if (state.menu.app_menu_open && last_menu_open_) {
      lv_group_focus_obj(tabs_);
    }
    return;
  }

  ModalKind kind = ModalKind::None;
  String title;
  String detail;
  std::array<String, 4> options{};
  size_t option_count = 0;
  size_t selected_index = 0;

  if (approval_open) {
    kind = ModalKind::Approval;
    title = state.approval_title.length() > 0 ? state.approval_title : "Approval requested";
    detail = state.approval_detail_line;
    options[0] = "Accept";
    options[1] = "Reject";
    option_count = 2;
    selected_index = 0;
  } else {
    switch (state.bridge_prompt_kind) {
      case BridgePromptKind::None:
        break;
      case BridgePromptKind::Notification:
        kind = ModalKind::Notification;
        title = state.bridge_prompt_title.length() > 0 ? state.bridge_prompt_title : "Notification";
        detail = state.bridge_prompt_detail;
        option_count = 1;
        options[0] = "OK";
        selected_index = 0;
        break;
      case BridgePromptKind::Question:
        kind = ModalKind::Question;
        title = state.bridge_prompt_title.length() > 0 ? state.bridge_prompt_title : "Question";
        detail = state.bridge_prompt_detail;
        option_count = state.bridge_prompt_option_count;
        for (size_t i = 0; i < option_count && i < options.size(); ++i) {
          options[i] = state.bridge_prompt_options[i];
        }
        if (option_count == 0) {
          options[0] = "Yes";
          options[1] = "No";
          option_count = 2;
        }
        selected_index = state.bridge_prompt_selected_index < option_count ? state.bridge_prompt_selected_index : 0;
        break;
      case BridgePromptKind::Confirmation:
        kind = ModalKind::Confirmation;
        title = state.bridge_prompt_title.length() > 0 ? state.bridge_prompt_title : "Confirmation";
        detail = state.bridge_prompt_detail;
        options[0] = "Accept";
        options[1] = "Reject";
        option_count = 2;
        selected_index = state.bridge_prompt_selected_index < option_count ? state.bridge_prompt_selected_index : 0;
        break;
    }
  }

  if (kind == ModalKind::None) {
    modal_.setVisible(false);
    return;
  }

  modal_.setContent(kind, title, detail, options, option_count, selected_index);
  modal_.focus();
}

void LvglScreen::syncPttState(const DeviceState& state) {
  (void)state;
  ptt_.setVisible(false);
}

void LvglScreen::syncContentScreen(const DeviceState& state) {
  if (content_root_ == nullptr) {
    return;
  }

  const bool content_open = navigation_.content_open;
  LvglAppScreen* desired_screen = content_open
                                    ? screenForApp(state.active_app,
                                                   buddy_screen_,
                                                   push_screen_,
                                                   runs_screen_,
                                                   approvals_screen_,
                                                   pager_screen_,
                                                   usage_screen_,
                                                   bridge_screen_,
                                                   settings_screen_)
                                    : nullptr;

  if (desired_screen != active_screen_) {
    if (active_screen_ != nullptr) {
      active_screen_->detach();
      active_screen_ = nullptr;
    }

    if (desired_screen != nullptr) {
      lv_obj_clear_flag(content_root_, LV_OBJ_FLAG_HIDDEN);
      desired_screen->attach(content_root_, port_.group());
      active_screen_ = desired_screen;
      active_screen_->onFocus();
    } else {
      lv_obj_add_flag(content_root_, LV_OBJ_FLAG_HIDDEN);
    }

    last_content_app_ = state.active_app;
    last_content_open_ = desired_screen != nullptr;
  }

  if (active_screen_ != nullptr) {
    active_screen_->sync(state);
  }
}

void LvglScreen::renderShell(const DeviceState& state, App& app, const String& input_line, const String& footer_hint) {
  if (root_ == nullptr) {
    begin();
  }

  updateNavigationState(state);
  syncMenuState(state);
  syncModalState(state);
  syncPttState(state);
  syncContentScreen(state);

  const bool modal_open = modal_.visible();
  const bool ptt_open = false;
  const bool content_open = last_content_open_;
  const bool menu_open = state.menu.app_menu_open && !modal_open;

  const size_t active_index = tabIndexForApp(state.active_app);
  const size_t focus_index = modal_open
                               ? modal_.selectedIndex()
                               : state.menu.app_menu_open ? state.menu.app_menu_selected : active_index;

  const String title = state.menu.app_menu_open ? "Applications" : "Cardputer Codex";
  setLabelText(title_, last_title_, title);

  if (menu_open || modal_open) {
    lv_obj_add_flag(active_app_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(status_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(detail_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(wifi_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(codex_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(battery_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(active_app_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(status_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(detail_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(wifi_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(codex_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(battery_, LV_OBJ_FLAG_HIDDEN);
  }

  if (content_open) {
    lv_obj_add_flag(status_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(detail_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(focus_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(footer_, LV_OBJ_FLAG_HIDDEN);
  } else if (!menu_open && !modal_open) {
    lv_obj_clear_flag(status_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(detail_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(focus_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(footer_, LV_OBJ_FLAG_HIDDEN);
  } else {
    // Menu or Modal: keep focus and footer but hide content areas
    lv_obj_clear_flag(focus_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(footer_, LV_OBJ_FLAG_HIDDEN);
  }

  String active;
  if (modal_open) {
    active = modal_.kind() == ModalKind::Approval ? "Approval"
            : modal_.kind() == ModalKind::Question ? "Question"
            : modal_.kind() == ModalKind::Confirmation ? "Confirmation"
            : modal_.kind() == ModalKind::Notification ? "Notification"
            : "Modal";
  } else if (content_open) {
    active = state.active_app == AppId::Runs ? "Runs dashboard" : state.active_app == AppId::Approvals ? "Approvals inbox" : "Buddy dashboard";
  } else if (ptt_open) {
    active = String("Recording: ") + (state.ptt_state == PushToTalkState::Ready ? "Ready"
                                         : state.ptt_state == PushToTalkState::Recording ? "Live"
                                         : state.ptt_state == PushToTalkState::Error ? "Error"
                                         : state.ptt_state == PushToTalkState::Armed ? "Armed"
                                         : "Idle");
  } else {
    active = state.menu.app_menu_open ? String("Current: ") + active_app_label(state.active_app)
                                      : active_app_label(state.active_app);
  }
  setLabelText(active_app_, last_active_app_, active);

  String status = state.status_line.length() > 0 ? state.status_line : String("Ready");
  if (state.menu.command_palette_open && input_line.length() > 0) {
    status = String("Cmd: ") + input_line;
  }
  if (ptt_open && state.ptt_detail_line.length() > 0) {
    status = state.ptt_detail_line;
  }
  if (content_open) {
    status = state.wifi_connected ? "Wi-Fi connected" : "Wi-Fi offline";
  }
  setLabelText(status_, last_status_, short_status(status, 48));

  String detail = String("Mode ") + ui_mode_label(state.ui_mode);
  detail += " | ";
  detail += codex_label(state.codex_state);
  if (ptt_open) {
    detail += " | ";
    detail += "PTT ";
    detail += state.ptt_samples_captured;
    detail += "/";
    detail += state.ptt_sample_limit;
    detail += " peak ";
    detail += state.ptt_peak_amplitude;
  }
  if (state.bridge_status_line.length() > 0) {
    detail += " | ";
    detail += state.bridge_status_line;
  }
  if (content_open && state.approval_pending) {
    detail += " | Approval pending";
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
  if (ptt_open) {
    footer = String("Space: record  Enter: send  Del: cancel");
  }
  if (content_open) {
    footer = state.active_app == AppId::Runs ? String("Runs dashboard") : state.active_app == AppId::Approvals ? String("Approvals inbox") : String("Buddy dashboard");
  }
  setLabelText(footer_, last_footer_, footer);

  if (modal_open) {
    String modal_line = String(modal_.kind() == ModalKind::Approval ? "Approval"
                               : modal_.kind() == ModalKind::Question ? "Question"
                               : modal_.kind() == ModalKind::Confirmation ? "Confirmation"
                               : modal_.kind() == ModalKind::Notification ? "Notification"
                               : "Modal");
    const char* choice = modal_.selectedLabel();
    if (choice != nullptr && String(choice).length() > 0) {
      modal_line += " | ";
      modal_line += choice;
    }
    setLabelText(focus_, last_focus_, modal_line);
  } else if (state.menu.command_palette_open) {
    setLabelText(focus_, last_focus_, "Focus: command palette");
  } else if (ptt_open) {
    String label = String("Focus: ");
    label += state.ptt_state == PushToTalkState::Ready ? "ready"
            : state.ptt_state == PushToTalkState::Recording ? "recording"
            : state.ptt_state == PushToTalkState::Error ? "error"
            : state.ptt_state == PushToTalkState::Armed ? "armed"
            : "idle";
    setLabelText(focus_, last_focus_, label);
  } else if (content_open) {
    setLabelText(focus_, last_focus_, state.active_app == AppId::Runs ? "Focus: Runs" : state.active_app == AppId::Approvals ? "Focus: Approvals" : "Focus: dashboard");
  } else if (state.menu.app_menu_open) {
    const char* focus_text = tabLabel(focus_index);
    String label = String("Focus: ") + focus_text;
    setLabelText(focus_, last_focus_, label);
  } else {
    const String label = String("Focus: ") + app.title();
    setLabelText(focus_, last_focus_, label);
  }
}

void LvglScreen::pushKey(uint32_t key, bool pressed) {
  port_.pushKey(static_cast<lv_key_t>(key), pressed);
}

void LvglScreen::updateNavigationState(const DeviceState& state) {
  navigation_.previous = navigation_.current;
  navigation_.app = state.active_app;
  navigation_.menu_open = state.menu.app_menu_open;
  navigation_.modal_open = state.approval_pending || state.bridge_prompt_kind != BridgePromptKind::None;
  navigation_.overlay = state.approval_pending ? OverlayKind::Approval
                                              : state.bridge_prompt_kind != BridgePromptKind::None ? OverlayKind::BridgePrompt
                                                                                                    : OverlayKind::None;
  if (navigation_.modal_open) {
    navigation_.current = ScreenId::Modal;
  } else if (navigation_.menu_open) {
    navigation_.current = ScreenId::Menu;
  } else if (state.active_app != AppId::Buddy) {
    navigation_.current = ScreenId::Content;
  } else {
    navigation_.current = ScreenId::Home;
  }
  navigation_.content_open = navigation_.current == ScreenId::Content;
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
