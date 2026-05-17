#include "app_shell.h"
#include "device_config.h"

namespace {
String trimmed_copy(const String& input) {
  String output = input;
  output.trim();
  return output;
}
}  // namespace

void AppShell::begin() {
  state_.firmware_name = "cardputer-codex-terminal";
  state_.active_app = AppId::Buddy;
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
  append_activity_event(state_, "Booted and waiting for middleware");

  input_line_ = "";
  network_.begin();
  network_.logMessage("App shell booted");
  const RuntimeNetworkConfig& runtime = network_.config();
  if (runtime.middleware_host.length() > 0) {
    bridge_.configure(runtime.middleware_host, runtime.middleware_port, runtime.middleware_path);
    if (runtime.middleware_token.length() > 0) {
      state_.bridge_status_line = "Middleware bridge configured from SD";
    }
    network_.logMessage(String("Middleware bridge configured from SD host ") + runtime.middleware_host);
  } else if (String(CARDPUTER_MIDDLEWARE_HOST).length() > 0) {
    bridge_.configure(CARDPUTER_MIDDLEWARE_HOST, static_cast<uint16_t>(CARDPUTER_MIDDLEWARE_PORT), CARDPUTER_MIDDLEWARE_PATH);
    network_.logMessage(String("Middleware bridge configured from firmware defaults host ") + CARDPUTER_MIDDLEWARE_HOST);
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
  screen_.begin();
  switchTo(AppId::Buddy);
}

void AppShell::handleCommand(const String& command) {
  const String trimmed = trimmed_copy(command);
  if (trimmed.length() == 0) {
    return;
  }

  if (trimmed == "/help") {
    Serial.println("Commands:");
    Serial.println("  /app buddy|push|pager|mcp");
    Serial.println("  /wifi on|off");
    Serial.println("  /wifi reload");
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
    Serial.println("  anything else is forwarded to the active app");
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
    } else if (value == "mcp") {
      switchTo(AppId::McpBridge);
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
        bridge_.configure(runtime.middleware_host, runtime.middleware_port, runtime.middleware_path);
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
    state_.codex_usage_label = "codex";
    state_.codex_usage_window_minutes = 15;
    state_.codex_usage_resets_at = 0;
    state_.codex_usage_detail_line = String("Usage ") + String(state_.codex_usage_percent) + "%";
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
}

void AppShell::render() {
  traceDisplay();
  screen_.renderShell(state_, *active_app_, input_line_);
}

void AppShell::handleKeyboardInput(const String& typed, bool submit, bool backspace) {
  if (backspace && input_line_.length() > 0) {
    input_line_.remove(input_line_.length() - 1);
  }

  if (typed.length() > 0) {
    input_line_ += typed;
  }

  if (submit) {
    const String submitted = input_line_;
    input_line_ = "";
    append_activity_event(state_, String("Submitted command: ") + submitted);
    handleCommand(submitted);
    return;
  }

  render();
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
  state_.status_line = accepted ? "Bridge prompt accepted" : "Bridge prompt rejected";
  append_activity_event(state_, state_.status_line);
  render();
}

bool AppShell::isPushToCodexActive() const {
  return state_.active_app == AppId::PushToCodex;
}

MiddlewareLink& AppShell::bridge() {
  return bridge_;
}

void AppShell::switchTo(AppId app_id) {
  if (active_app_ != nullptr) {
    active_app_->onExit(state_);
  }

  state_.active_app = app_id;
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
    case AppId::McpBridge:
      active_app_ = &mcp_bridge_app_;
      break;
  }

  if (active_app_ != nullptr) {
    active_app_->onEnter(state_);
    append_activity_event(state_, String("Active app: ") + active_app_->title());
  }

  push_to_codex_app_.setBridge(&bridge_);
  mcp_bridge_app_.setBridge(&bridge_);
}

void AppShell::traceDisplay() {
  String trace;
  trace.reserve(240);
  trace += "display app=";
  trace += active_app_ != nullptr ? active_app_->title() : "none";
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
