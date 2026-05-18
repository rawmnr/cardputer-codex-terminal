#include "app_shell.h"

#include "device_config.h"
#include "string_print.h"

namespace {
String trimmed_copy(const String& input) {
  String output = input;
  output.trim();
  return output;
}

String tab_label(AppId app_id) {
  switch (app_id) {
    case AppId::Buddy:
      return "Buddy";
    case AppId::PushToCodex:
      return "Push";
    case AppId::Pager:
      return "Pager";
    case AppId::Usage:
      return "Usage";
    case AppId::McpBridge:
      return "MCP";
    case AppId::Settings:
      return "Settings";
  }
  return "App";
}

const char* menu_label(AppId app_id) {
  switch (app_id) {
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
}  // namespace

void AppShell::begin() {
  state_.firmware_name = "cardputer-codex-terminal";
  state_.active_app = AppId::Buddy;
  state_.ui_mode = UiMode::Home;
  state_.menu.active_tab = 0;
  state_.menu.app_menu_selected = 0;
  state_.menu.selected_index = 0;
  state_.menu.scroll_offset = 0;
  state_.menu.app_menu_open = false;
  state_.menu.command_palette_open = false;
  state_.battery_percent = 0;
  state_.battery_voltage_mv = 0;
  state_.wifi_connected = false;
  state_.wifi_ssid = "";
  state_.wifi_ip = "";
  state_.network_status_line = "Loading Wi-Fi config from SD";
  state_.codex_state = CodexState::Idle;
  state_.status_line = "Waiting for middleware connection";
  state_.codex_workspace_path = ".";
  state_.codex_branch = "";
  state_.codex_thread_id = "";
  state_.approval_id = "";
  state_.approval_title = "";
  state_.approval_detail_line = "";
  state_.approval_timeout_seconds = 0;
  state_.approval_pending = false;
  state_.bridge_prompt_kind = BridgePromptKind::None;
  state_.bridge_prompt_pending = false;
  state_.bridge_prompt_title = "";
  state_.bridge_prompt_detail = "";
  state_.bridge_prompt_option_count = 0;
  state_.bridge_prompt_selected_index = 0;
  state_.codex_stream_line = "";
  state_.bridge_status_line = "Middleware bridge not configured";
  state_.pager.session_count = 0;
  state_.pager.active_session_id = "";
  state_.pager.selected_session_id = "";
  state_.pager.interrupt_supported = false;
  state_.pager_screen = PagerScreen::Inbox;
  append_activity_event(state_, "Booted and waiting for middleware");

  input_line_ = "";
  state_.last_interaction_ms = millis();
  network_.begin();
  network_.logMessage("App shell booted");
  const RuntimeNetworkConfig& runtime = network_.config();
  
  // Use SD config if available, otherwise firmware defaults, otherwise empty (for mDNS discovery)
  String host = runtime.middleware_host;
  uint16_t port = runtime.middleware_port;
  String path = runtime.middleware_path;
  String token = runtime.middleware_token;

  if (host.length() == 0 && String(CARDPUTER_MIDDLEWARE_HOST).length() > 0) {
    host = CARDPUTER_MIDDLEWARE_HOST;
    port = static_cast<uint16_t>(CARDPUTER_MIDDLEWARE_PORT);
    path = CARDPUTER_MIDDLEWARE_PATH;
    token = CARDPUTER_MIDDLEWARE_TOKEN;
    network_.logMessage(String("Using firmware default host ") + host);
  }

  bridge_.configure(host, port, path, token);
  
  if (host.length() > 0) {
    state_.bridge_status_line = "Middleware bridge configured";
    network_.logMessage(String("Middleware bridge configured with host ") + host);
  } else {
    state_.bridge_status_line = "Auto-discovering bridge...";
    network_.logMessage("No bridge host configured, will use mDNS discovery");
  }

  if (runtime.sd_mounted) {
    if (runtime.sd_config_loaded) {
      append_activity_event(state_, "Loaded configuration from SD");
    } else {
      append_activity_event(state_, "SD mounted with no config file");
    }
  } else {
    append_activity_event(state_, "SD card not mounted");
  }

  bridge_.begin(state_);
  network_.tick(state_);
#if USE_LVGL_UI
  lvgl_screen_.begin();
#else
  screen_.begin();
#endif
  push_to_codex_app_.setBridge(&bridge_);
  pager_app_.setBridge(&bridge_);
  mcp_bridge_app_.setBridge(&bridge_);
  switchTo(AppId::Buddy);
}

void AppShell::handleCommand(const String& command) {
  const String trimmed = trimmed_copy(command);
  if (trimmed.length() == 0) {
    return;
  }

  if (trimmed == "/help") {
    Serial.println("Menu-first UI:");
    Serial.println("  Ctrl-M             open app menu");
    Serial.println("  Fn+; / Fn+.        move selection");
    Serial.println("  Enter              select / approve");
    Serial.println("  Del                back / reject");
    Serial.println("  Space              contextual action");
    Serial.println("  Hold Space         push-to-talk");
    Serial.println("  /                  command palette");
    Serial.println("Advanced commands:");
    Serial.println("  /app buddy|push|pager|usage|mcp|settings");
    Serial.println("  /wifi on|off|reload");
    Serial.println("  /codex idle|busy|approval|offline");
    Serial.println("  /workspace <path>");
    Serial.println("  /branch <name>");
    Serial.println("  /thread <id>");
    Serial.println("  /approval <detail>");
    Serial.println("  /approve | /reject");
    Serial.println("  /notify <detail>");
    Serial.println("  /ask <title> | <detail> | <opt1> | <opt2> | <opt3>");
    Serial.println("  /confirm <detail>");
    Serial.println("  /bridge select <1-3> | /bridge accept | /bridge reject");
    Serial.println("  /battery <0-100>");
    Serial.println("  /status <text>");
    return;
  }

  if (trimmed.startsWith("/app ")) {
    const String value = trimmed.substring(5);
    if (value == "buddy") {
      switchTo(AppId::Buddy);
    } else if (value == "push") {
      switchTo(AppId::PushToCodex);
    } else if (value == "pager") {
      switchTo(AppId::Pager);
    } else if (value == "usage") {
      switchTo(AppId::Usage);
    } else if (value == "mcp") {
      switchTo(AppId::McpBridge);
    } else if (value == "settings") {
      switchTo(AppId::Settings);
    } else {
      Serial.println("Unknown app.");
      return;
    }
    append_activity_event(state_, String("Switched to ") + active_app_->title());
    render();
    return;
  }

  if (trimmed.startsWith("/wifi ")) {
    if (trimmed == "/wifi reload") {
      network_.begin();
      network_.logMessage("Wi-Fi configuration reloaded from SD");
      const RuntimeNetworkConfig& runtime = network_.config();
      if (runtime.middleware_host.length() > 0) {
        bridge_.configure(runtime.middleware_host, runtime.middleware_port, runtime.middleware_path, runtime.middleware_token);
      } else if (String(CARDPUTER_MIDDLEWARE_HOST).length() > 0) {
        bridge_.configure(
          CARDPUTER_MIDDLEWARE_HOST,
          static_cast<uint16_t>(CARDPUTER_MIDDLEWARE_PORT),
          CARDPUTER_MIDDLEWARE_PATH,
          CARDPUTER_MIDDLEWARE_TOKEN
        );
      }
      state_.status_line = "Wi-Fi and SD config reloaded";
      append_activity_event(state_, state_.status_line);
      render();
      return;
    }
    state_.wifi_connected = trimmed.endsWith("on");
    state_.status_line = state_.wifi_connected ? "Wi-Fi connected" : "Wi-Fi offline";
    append_activity_event(state_, state_.status_line);
    render();
    return;
  }

  if (trimmed.startsWith("/codex ")) {
    const String value = trimmed.substring(7);
    if (value == "idle") {
      state_.codex_state = CodexState::Idle;
    } else if (value == "busy") {
      state_.codex_state = CodexState::Busy;
    } else if (value == "approval") {
      state_.codex_state = CodexState::WaitingForApproval;
    } else if (value == "offline") {
      state_.codex_state = CodexState::Offline;
    }
    append_activity_event(state_, String("Codex state: ") + value);
    render();
    return;
  }

  if (trimmed.startsWith("/workspace ")) {
    state_.codex_workspace_path = trimmed.substring(11);
    state_.codex_thread_id = "";
    append_activity_event(state_, String("Workspace: ") + state_.codex_workspace_path);
    bridge_.sendStatusRequest();
    render();
    return;
  }

  if (trimmed.startsWith("/branch ")) {
    state_.codex_branch = trimmed.substring(8);
    append_activity_event(state_, String("Branch: ") + state_.codex_branch);
    bridge_.sendStatusRequest();
    render();
    return;
  }

  if (trimmed.startsWith("/thread ")) {
    state_.codex_thread_id = trimmed.substring(8);
    append_activity_event(state_, String("Thread: ") + state_.codex_thread_id);
    bridge_.sendStatusRequest();
    render();
    return;
  }

  if (trimmed.startsWith("/approval ")) {
    state_.approval_pending = true;
    state_.approval_id = "local-approval";
    state_.approval_title = "Approval requested";
    state_.approval_detail_line = trimmed.substring(10);
    state_.approval_timeout_seconds = 0;
    state_.codex_state = CodexState::WaitingForApproval;
    state_.ui_mode = UiMode::Approval;
    state_.status_line = "Approval pending";
    append_activity_event(state_, String("Approval requested: ") + state_.approval_detail_line);
    render();
    return;
  }

  if (trimmed == "/approve") {
    handleApprovalDecision(true);
    return;
  }

  if (trimmed == "/reject") {
    handleApprovalDecision(false);
    return;
  }

  if (trimmed.startsWith("/notify ") || trimmed.startsWith("/ask ") || trimmed.startsWith("/confirm ") || trimmed.startsWith("/bridge ")) {
    if (state_.active_app == AppId::McpBridge && active_app_ != nullptr) {
      active_app_->onCommand(trimmed, state_);
      render();
      return;
    }
    state_.status_line = "Switch to MCP Bridge to use bridge commands";
    append_activity_event(state_, state_.status_line);
    render();
    return;
  }

  if (trimmed.startsWith("/battery ")) {
    const int value = trimmed.substring(9).toInt();
    state_.battery_percent = constrain(value, 0, 100);
    append_activity_event(state_, String("Battery set to ") + String(state_.battery_percent) + "%");
    render();
    return;
  }

  if (trimmed.startsWith("/status ")) {
    state_.status_line = trimmed.substring(8);
    append_activity_event(state_, String("Status: ") + state_.status_line);
    render();
    return;
  }

  if (trimmed.startsWith("/usage ")) {
    const int value = trimmed.substring(7).toInt();
    state_.codex_usage_percent = constrain(value, 0, 100);
    state_.codex_usage_secondary_percent = constrain(value / 2, 0, 100);
    state_.codex_usage_label = "codex";
    state_.codex_usage_window_minutes = 15;
    state_.codex_usage_secondary_window_minutes = 10080;
    state_.codex_usage_resets_at = millis() / 1000 + 300;
    state_.codex_usage_secondary_resets_at = millis() / 1000 + 3600;
    state_.codex_usage_reset_line = "Resets in 5m / 60m";
    state_.codex_usage_detail_line = String("Primary ") + String(state_.codex_usage_percent) + "%";
    append_activity_event(state_, String("Usage set to ") + String(state_.codex_usage_percent) + "%");
    render();
    return;
  }

  if (active_app_ != nullptr) {
    if (state_.active_app == AppId::PushToCodex) {
      active_app_->onSubmit(trimmed, state_);
    } else {
      active_app_->onCommand(trimmed, state_);
    }
    render();
  }
}

void AppShell::tick() {
  state_.battery_percent = M5Cardputer.Power.getBatteryLevel();
  state_.battery_voltage_mv = M5Cardputer.Power.getBatteryVoltage();
  network_.tick(state_);
  bridge_.tick(state_);

  if (active_app_ != nullptr) {
    active_app_->tick(state_);
  }

#if USE_LVGL_UI
  lvgl_screen_.tick();
#endif

  // Power management
  const unsigned long now = millis();
  const unsigned long idle_time = now - state_.last_interaction_ms;
  
  if (idle_time > 120000) { // 2 minutes: Light sleep
    M5Cardputer.Display.setBrightness(0);
    // Note: Light sleep on ESP32-S3 can interfere with WiFi/WebSockets if not handled carefully.
    // For now, we just dim to 0 to save significant power without dropping the link.
  } else if (idle_time > 30000) { // 30 seconds: Dim
    M5Cardputer.Display.setBrightness(32);
  } else {
    M5Cardputer.Display.setBrightness(128);
  }
}

void AppShell::render() {
  traceDisplay();
#if USE_LVGL_UI
  if (active_app_ != nullptr) {
    lvgl_screen_.renderShell(state_, *active_app_, input_line_, footerHint());
  }
#else
  screen_.renderShell(state_, *active_app_, input_line_, footerHint());
#endif
  emitDisplaySnapshot();
}

#if USE_LVGL_UI
void AppShell::handleUiKey(lv_key_t key, bool pressed) {
  lvgl_screen_.pushKey(key, pressed);
}
#endif

void AppShell::handleTextInput(const String& typed, bool submit, bool backspace) {
  if (state_.menu.app_menu_open) {
    return;
  }

  const bool command_palette = state_.menu.command_palette_open;
  const bool app_text_mode =
    state_.active_app == AppId::PushToCodex ||
    (state_.active_app == AppId::Pager && state_.pager_screen == PagerScreen::Compose);

  if (!command_palette && !app_text_mode) {
    state_.menu.command_palette_open = true;
    state_.ui_mode = UiMode::Input;
    input_line_ = "";
  }

  if (state_.menu.command_palette_open) {
    if (backspace && input_line_.length() > 0) {
      input_line_.remove(input_line_.length() - 1);
    }

    if (typed.length() > 0) {
      input_line_ += typed;
    }

    if (submit) {
      const String submitted = trimmed_copy(input_line_);
      input_line_ = "";
      state_.menu.command_palette_open = false;
      state_.ui_mode = UiMode::Home;
      if (submitted.length() > 0) {
        append_activity_event(state_, String("Submitted command: ") + submitted);
        handleCommand(submitted);
      } else {
        render();
      }
      return;
    }

    render();
    return;
  }

  if (active_app_ != nullptr) {
    if (backspace) {
      active_app_->onTextInput("", true, state_);
    }

    if (typed.length() > 0) {
      active_app_->onTextInput(typed, false, state_);
    }

    if (submit) {
      active_app_->onSubmit("", state_);
    }
    render();
  }
}

void AppShell::handleAction(UiAction action) {
  if (action == UiAction::None) {
    return;
  }

  if (state_.approval_pending) {
    if (action == UiAction::Select) {
      handleApprovalDecision(true);
    } else if (action == UiAction::Back) {
      handleApprovalDecision(false);
    }
    return;
  }

  if (state_.bridge_prompt_pending) {
    if (action == UiAction::Up && state_.bridge_prompt_option_count > 0) {
      if (state_.bridge_prompt_selected_index == 0) {
        state_.bridge_prompt_selected_index = state_.bridge_prompt_option_count - 1;
      } else {
        state_.bridge_prompt_selected_index--;
      }
      state_.status_line = String("Bridge option selected: ") +
                           state_.bridge_prompt_options[state_.bridge_prompt_selected_index];
      append_activity_event(state_, state_.status_line);
      render();
      return;
    }

    if (action == UiAction::Down && state_.bridge_prompt_option_count > 0) {
      state_.bridge_prompt_selected_index = (state_.bridge_prompt_selected_index + 1) % state_.bridge_prompt_option_count;
      state_.status_line = String("Bridge option selected: ") +
                           state_.bridge_prompt_options[state_.bridge_prompt_selected_index];
      append_activity_event(state_, state_.status_line);
      render();
      return;
    }

    if (action == UiAction::Select) {
      handleBridgePromptDecision(true);
      return;
    }

    if (action == UiAction::Back) {
      handleBridgePromptDecision(false);
      return;
    }
  }

  if (state_.menu.command_palette_open && action == UiAction::Back) {
    input_line_ = "";
    state_.menu.command_palette_open = false;
    state_.ui_mode = UiMode::Home;
    state_.status_line = "Command palette closed";
    append_activity_event(state_, state_.status_line);
    render();
    return;
  }

  if (action == UiAction::Menu) {
    state_.menu.command_palette_open = false;
    input_line_ = "";
    state_.menu.app_menu_open = !state_.menu.app_menu_open;
    if (state_.menu.app_menu_open) {
      state_.menu.app_menu_selected = tabIndexForApp(state_.active_app);
      state_.ui_mode = UiMode::Menu;
    } else {
      state_.ui_mode = UiMode::Home;
    }
    render();
    return;
  }

  if (state_.menu.app_menu_open) {
    if (action == UiAction::Up) {
      state_.menu.app_menu_selected = state_.menu.app_menu_selected == 0 ? 5 : state_.menu.app_menu_selected - 1;
      render();
      return;
    }

    if (action == UiAction::Down) {
      state_.menu.app_menu_selected = (state_.menu.app_menu_selected + 1) % 6;
      render();
      return;
    }

    if (action == UiAction::Select) {
      setActiveTab(state_.menu.app_menu_selected);
      state_.menu.app_menu_open = false;
      state_.ui_mode = UiMode::Home;
      render();
      return;
    }

    if (action == UiAction::Back) {
      state_.menu.app_menu_open = false;
      state_.ui_mode = UiMode::Home;
      render();
      return;
    }

    return;
  }

  if (action == UiAction::Left || action == UiAction::Right) {
    return;
  }

  if (active_app_ != nullptr) {
    if (state_.active_app == AppId::Buddy && action == UiAction::Select) {
      setActiveTab(tabIndexForApp(AppId::Pager));
      render();
      return;
    }

    if (state_.active_app == AppId::Settings && action == UiAction::Back) {
      setActiveTab(tabIndexForApp(AppId::Buddy));
      render();
      return;
    }

    if (state_.active_app == AppId::McpBridge && action == UiAction::Back && !state_.bridge_prompt_pending) {
      setActiveTab(tabIndexForApp(AppId::Buddy));
      render();
      return;
    }

    if (state_.active_app == AppId::Pager && action == UiAction::Back && state_.pager_screen == PagerScreen::Inbox) {
      setActiveTab(tabIndexForApp(AppId::Buddy));
      render();
      return;
    }

    active_app_->onAction(action, state_);
    render();
  }
}

void AppShell::handlePushToTalk(bool pressed) {
  if (active_app_ != nullptr) {
    active_app_->onPushToTalk(pressed, state_);
    append_activity_event(state_, pressed ? "Push-to-talk pressed" : "Push-to-talk released");
    render();
  }
}

bool AppShell::hasPendingApproval() const {
  return state_.approval_pending;
}

bool AppShell::hasPendingBridgePrompt() const {
  return state_.bridge_prompt_pending;
}

void AppShell::handleApprovalDecision(bool approved) {
  if (!state_.approval_pending) {
    state_.status_line = approved ? "No approval pending to accept" : "No approval pending to reject";
    append_activity_event(state_, state_.status_line);
    render();
    return;
  }

  const String outcome = approved ? "Approval accepted" : "Approval rejected";
  state_.approval_pending = false;
  state_.approval_id = "";
  state_.approval_title = "";
  state_.approval_detail_line = "";
  state_.approval_timeout_seconds = 0;
  state_.codex_state = CodexState::Idle;
  state_.ui_mode = UiMode::Menu;
  state_.status_line = outcome;
  bridge_.sendApprovalResponse(approved);
  append_activity_event(state_, outcome);
  render();
}

void AppShell::handleBridgePromptDecision(bool accepted) {
  if (!state_.bridge_prompt_pending) {
    state_.status_line = accepted ? "No bridge prompt pending to accept" : "No bridge prompt pending to reject";
    append_activity_event(state_, state_.status_line);
    render();
    return;
  }

  const size_t selected_index = state_.bridge_prompt_selected_index;
  const String selected_text =
    selected_index < state_.bridge_prompt_option_count ? state_.bridge_prompt_options[selected_index] : "";
  bridge_.sendBridgeResponse(accepted, selected_index, selected_text);
  state_.bridge_prompt_pending = false;
  state_.bridge_prompt_kind = BridgePromptKind::None;
  state_.bridge_prompt_title = "";
  state_.bridge_prompt_detail = "";
  state_.bridge_prompt_option_count = 0;
  state_.bridge_prompt_selected_index = 0;
  state_.ui_mode = UiMode::Menu;
  state_.status_line = accepted ? "Bridge prompt accepted" : "Bridge prompt rejected";
  append_activity_event(state_, state_.status_line);
  render();
}

bool AppShell::isPushToCodexActive() const {
  return state_.active_app == AppId::PushToCodex;
}

UiMode AppShell::uiMode() const {
  return state_.ui_mode;
}

MiddlewareLink& AppShell::bridge() {
  return bridge_;
}

void AppShell::switchTo(AppId app_id) {
  if (active_app_ != nullptr) {
    active_app_->onExit(state_);
  }

  state_.active_app = app_id;
  state_.menu.active_tab = tabIndexForApp(app_id);
  state_.menu.app_menu_selected = state_.menu.active_tab;
  state_.menu.command_palette_open = false;
  input_line_ = "";

  switch (app_id) {
    case AppId::Buddy:
      active_app_ = &buddy_app_;
      break;
    case AppId::PushToCodex:
      active_app_ = &push_to_codex_app_;
      break;
    case AppId::Pager:
      active_app_ = &pager_app_;
      break;
    case AppId::Usage:
      active_app_ = &usage_app_;
      break;
    case AppId::McpBridge:
      active_app_ = &mcp_bridge_app_;
      break;
    case AppId::Settings:
      active_app_ = &settings_app_;
      break;
  }

  if (active_app_ != nullptr) {
    active_app_->onEnter(state_);
    append_activity_event(state_, String("Active app: ") + active_app_->title());
  }
}

void AppShell::setActiveTab(size_t tab_index) {
  const AppId app_id = appForTab(tab_index % 5);
  switchTo(app_id);
}

size_t AppShell::tabIndexForApp(AppId app_id) const {
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

AppId AppShell::appForTab(size_t tab_index) const {
  switch (tab_index % 6) {
    case 0:
      return AppId::Buddy;
    case 1:
      return AppId::PushToCodex;
    case 2:
      return AppId::Pager;
    case 3:
      return AppId::Usage;
    case 4:
      return AppId::McpBridge;
    case 5:
      return AppId::Settings;
  }
  return AppId::Buddy;
}

String AppShell::footerHint() const {
  if (state_.menu.app_menu_open) {
    return "Fn+;/. Move  Enter Open  Del Close";
  }

  if (state_.approval_pending) {
    return "Enter Approve  Del Reject";
  }

  if (state_.bridge_prompt_pending) {
    return "Fn+;/. Select  Enter OK  Del Reject";
  }

  if (state_.menu.command_palette_open) {
    return "Enter Run  Del Back  Ctrl-M Menu";
  }

  switch (state_.active_app) {
    case AppId::Buddy:
      return "Ctrl-M Menu  Enter Open Pager  / Cmd";
    case AppId::PushToCodex:
      return "Enter Send  Space Hold Talk  Del Back";
    case AppId::Pager:
      switch (state_.pager_screen) {
        case PagerScreen::Compose:
          return "Enter Send  Del Back  Ctrl-M Menu";
        case PagerScreen::Inbox:
          return "Fn+;/. Move  Enter Detail  Del Back";
        case PagerScreen::Detail:
          return "Fn+;/. Browse  Enter Reply  Del Back";
      }
      break;
    case AppId::Usage:
      return "Fn+;/. Refresh  Enter Buddy  Del Back";
    case AppId::McpBridge:
      return "Fn+;/. Select  Enter OK  Del Back";
    case AppId::Settings:
      return "Fn+;/. Move  Enter Inspect  Del Back";
  }

  return "Ctrl-M Menu  Enter Select  Del Back";
}

void AppShell::traceDisplay() {
  String trace;
  trace.reserve(280);
  trace += "display active=";
  trace += active_app_ != nullptr ? active_app_->title() : "none";
  trace += " id=";
  trace += tab_label(state_.active_app);
  trace += " menu=";
  trace += state_.menu.app_menu_open ? "open" : "closed";
  trace += " mode=";
  switch (state_.ui_mode) {
    case UiMode::Home:
      trace += "home";
      break;
    case UiMode::Menu:
      trace += "menu";
      break;
    case UiMode::Input:
      trace += "input";
      break;
    case UiMode::Modal:
      trace += "modal";
      break;
    case UiMode::Approval:
      trace += "approval";
      break;
    case UiMode::BridgePrompt:
      trace += "bridge";
      break;
  }
  trace += " wifi=";
  trace += state_.wifi_connected ? "connected" : "offline";
  trace += " net=\"";
  trace += state_.network_status_line;
  trace += "\" status=\"";
  trace += state_.status_line;
  trace += "\" input=\"";
  trace += input_line_;
  trace += "\" codex=";
  switch (state_.codex_state) {
    case CodexState::Offline:
      trace += "offline";
      break;
    case CodexState::Idle:
      trace += "idle";
      break;
    case CodexState::Busy:
      trace += "busy";
      break;
    case CodexState::WaitingForApproval:
      trace += "approval";
      break;
  }
  trace += " approval=";
  trace += state_.approval_pending ? "pending" : "clear";
  trace += " bridge=";
  trace += state_.bridge_prompt_pending ? "pending" : "clear";

  if (trace != last_display_trace_) {
    network_.logMessage(trace);
    last_display_trace_ = trace;
  }
}

void AppShell::emitDisplaySnapshot() {
  StringPrint out;
  out.println(state_.firmware_name);
  out.print("Net: ");
  out.println(state_.network_status_line);
  out.print("App: ");
  out.println(active_app_ != nullptr ? active_app_->title() : "none");
  out.print("Menu: ");
  out.println(state_.menu.app_menu_open ? "open" : "closed");
  out.print("Mode: ");
  switch (state_.ui_mode) {
    case UiMode::Home:
      out.println("home");
      break;
    case UiMode::Menu:
      out.println("menu");
      break;
    case UiMode::Input:
      out.println("input");
      break;
    case UiMode::Modal:
      out.println("modal");
      break;
    case UiMode::Approval:
      out.println("approval");
      break;
    case UiMode::BridgePrompt:
      out.println("bridge prompt");
      break;
  }
  out.print("Battery: ");
  out.print(state_.battery_percent);
  out.print("% / ");
  out.println(state_.battery_voltage_mv);
  out.print("Codex: ");
  switch (state_.codex_state) {
    case CodexState::Offline:
      out.println("offline");
      break;
    case CodexState::Idle:
      out.println("idle");
      break;
    case CodexState::Busy:
      out.println("busy");
      break;
    case CodexState::WaitingForApproval:
      out.println("waiting for approval");
      break;
  }
  if (state_.codex_stream_line.length() > 0) {
    out.print("Stream: ");
    out.println(state_.codex_stream_line);
  }
  if (state_.approval_pending) {
    out.print("Approval: ");
    out.println(state_.approval_title.length() > 0 ? state_.approval_title : "(untitled)");
    if (state_.approval_detail_line.length() > 0) {
      out.print("Approval detail: ");
      out.println(state_.approval_detail_line);
    }
  }
  if (state_.bridge_prompt_pending) {
    out.print("Bridge prompt: ");
    out.println(state_.bridge_prompt_title.length() > 0 ? state_.bridge_prompt_title : "(untitled)");
    if (state_.bridge_prompt_detail.length() > 0) {
      out.print("Bridge detail: ");
      out.println(state_.bridge_prompt_detail);
    }
  }
  out.println();
  if (active_app_ != nullptr) {
    active_app_->render(out, state_);
  }
  out.println();
  if (state_.menu.command_palette_open) {
    out.print("/ ");
    out.println(input_line_);
  } else {
    out.print("> ");
    out.println(input_line_);
  }

  if (!bridge_.isConnected()) {
    return;
  }

  bridge_.sendDisplaySnapshot(
    out.str(),
    state_.status_line,
    active_app_ != nullptr ? active_app_->title() : "",
    input_line_,
    state_.firmware_name,
    state_.network_status_line
  );
}
