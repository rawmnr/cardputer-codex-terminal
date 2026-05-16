#include "apps.h"

#include <array>
#include <M5Cardputer.h>
#include <mbedtls/base64.h>

#include "middleware_link.h"

namespace {
const char* app_label(AppId app_id) {
  switch (app_id) {
    case AppId::Buddy:
      return "Codex Buddy";
    case AppId::PushToCodex:
      return "Push to Codex";
    case AppId::Pager:
      return "Codex Pager";
    case AppId::McpBridge:
      return "Cardputer MCP Bridge";
  }
  return "Unknown";
}

String trim_copy(String value) {
  value.trim();
  return value;
}

void clear_bridge_prompt(DeviceState& state) {
  state.bridge_prompt_kind = BridgePromptKind::None;
  state.bridge_prompt_pending = false;
  state.bridge_prompt_title = "";
  state.bridge_prompt_detail = "";
  state.bridge_prompt_option_count = 0;
  state.bridge_prompt_selected_index = 0;
  for (size_t i = 0; i < state.bridge_prompt_options.size(); ++i) {
    state.bridge_prompt_options[i] = "";
  }
}

void set_bridge_options(DeviceState& state, const std::array<String, 3>& options, size_t count) {
  state.bridge_prompt_option_count = count;
  state.bridge_prompt_selected_index = 0;
  for (size_t i = 0; i < state.bridge_prompt_options.size(); ++i) {
    state.bridge_prompt_options[i] = i < count ? options[i] : "";
  }
}

void print_common_footer(Print& out) {
  out.println();
  out.println("Commands: /app buddy|push|pager|mcp | /wifi on|off | /codex idle|busy|approval|offline");
  out.println("          /workspace <path> | /branch <name> | /thread <id>");
  out.println("          /battery <0-100> | /usage <0-100> | /status <text> | /help");
}
}  // namespace

const char* BuddyApp::title() const { return "Codex Buddy"; }

void BuddyApp::onEnter(DeviceState& state) {
  state.status_line = "Buddy mode active";
}

void BuddyApp::onExit(DeviceState& state) {
  (void)state;
}

void BuddyApp::onCommand(const String& command, DeviceState& state) {
  (void)command;
  state.status_line = "Buddy view ready for live status";
}

void BuddyApp::tick(DeviceState& state) {
  (void)state;
}

void BuddyApp::render(Print& out, const DeviceState& state) {
  out.println("=== Codex Buddy ===");
  out.print("Wi-Fi: ");
  out.println(state.wifi_connected ? "connected" : "offline");
  out.print("Net: ");
  out.println(state.network_status_line);
  out.print("Usage: ");
  if (state.codex_usage_percent >= 0) {
    out.print(state.codex_usage_percent);
    out.print("%");
    if (state.codex_usage_window_minutes > 0) {
      out.print(" / ");
      out.print(state.codex_usage_window_minutes);
      out.println("m window");
    } else {
      out.println();
    }
  } else {
    out.println("unknown");
  }
  out.print("Codex: ");
  switch (state.codex_state) {
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
  out.print("Battery: ");
  out.print(state.battery_percent);
  out.println("%");
  out.print("Session: ");
  out.println(state.codex_workspace_path);
  out.print("Branch: ");
  out.println(state.codex_branch.length() > 0 ? state.codex_branch : "(none)");
  out.print("Thread: ");
  out.println(state.codex_thread_id.length() > 0 ? state.codex_thread_id : "(none)");
  out.print("Bridge: ");
  out.println(state.bridge_status_line.length() > 0 ? state.bridge_status_line : "(idle)");
  if (state.codex_stream_line.length() > 0) {
    out.print("Last: ");
    out.println(state.codex_stream_line);
  }
  out.print("Approval: ");
  out.println(state.approval_pending ? "pending" : "clear");
  if (state.approval_pending) {
    out.print("Request: ");
    out.println(state.approval_title.length() > 0 ? state.approval_title : "(untitled)");
    if (state.approval_detail_line.length() > 0) {
      out.print("Detail: ");
      out.println(state.approval_detail_line);
    }
  }
  out.print("Status: ");
  out.println(state.status_line);
  print_common_footer(out);
}

const char* PushToCodexApp::title() const { return "Push to Codex"; }

void PushToCodexApp::onEnter(DeviceState& state) {
  draft_ = "";
  captured_sample_count_ = 0;
  chunk_index_ = 0;
  peak_amplitude_ = 0;
  recording_ = false;
  mic_started_ = false;
  state.ptt_state = PushToTalkState::Armed;
  state.ptt_samples_captured = 0;
  state.ptt_sample_limit = kMaxSamples;
  state.ptt_sample_rate_hz = 16000;
  state.ptt_peak_amplitude = 0;
  state.ptt_detail_line = "Hold SPACE to record a voice prompt";
  state.status_line = "Hold SPACE to record a voice prompt";
  append_activity_event(state, "Push-to-talk armed");
  if (!M5.Mic.isEnabled()) {
    state.ptt_state = PushToTalkState::Error;
    state.ptt_detail_line = "Microphone not enabled";
    state.status_line = "Microphone unavailable";
    append_activity_event(state, "Microphone unavailable");
    return;
  }

  M5.Mic.setSampleRate(16000);
  if (M5.Mic.begin()) {
    mic_started_ = true;
    append_activity_event(state, "Microphone ready");
  } else {
    state.ptt_state = PushToTalkState::Error;
    state.ptt_detail_line = "Failed to start microphone";
    state.status_line = "Mic start failed";
    append_activity_event(state, "Failed to start microphone");
  }
}

void PushToCodexApp::onExit(DeviceState& state) {
  (void)state;
}

void PushToCodexApp::onCommand(const String& command, DeviceState& state) {
  draft_ = command;
  state.status_line = "Prompt staged for delivery";
}

void PushToCodexApp::onSubmit(const String& command, DeviceState& state) {
  draft_ = command;
  state.status_line = "Text prompt staged for middleware bridge";
  if (bridge_ != nullptr && bridge_->sendTextPrompt(command)) {
    state.status_line = "Text prompt sent to middleware";
    append_activity_event(state, "Text prompt sent to middleware");
  }
}

void PushToCodexApp::setBridge(MiddlewareLink* bridge) {
  bridge_ = bridge;
}

void PushToCodexApp::onPushToTalk(bool pressed, DeviceState& state) {
  if (state.ptt_state == PushToTalkState::Error) {
    return;
  }

  if (pressed) {
    beginRecording(state);
  } else {
    finishRecording(state);
  }
}

void PushToCodexApp::tick(DeviceState& state) {
  if (!recording_) {
    updatePttState(state);
    return;
  }

  if (!mic_started_) {
    state.ptt_state = PushToTalkState::Error;
    state.ptt_detail_line = "Microphone is not ready";
    state.status_line = "Microphone is not ready";
    append_activity_event(state, "Microphone is not ready");
    recording_ = false;
    return;
  }

  if (captured_sample_count_ >= kMaxSamples) {
    state.ptt_state = PushToTalkState::Ready;
    state.ptt_detail_line = "Capture buffer full";
    state.status_line = "Capture buffer full";
    append_activity_event(state, "Capture buffer full");
    recording_ = false;
    return;
  }

  const size_t remaining = kMaxSamples - captured_sample_count_;
  const size_t chunk_length = remaining < kChunkSamples ? remaining : kChunkSamples;
  if (chunk_length == 0) {
    state.ptt_state = PushToTalkState::Ready;
    state.ptt_detail_line = "Capture complete";
    state.status_line = "Capture complete";
    append_activity_event(state, "Capture complete");
    recording_ = false;
    return;
  }

  if (M5.Mic.record(chunk_buffer_.data(), chunk_length, 16000)) {
    appendChunk(chunk_buffer_.data(), chunk_length, state);
  }
  updatePttState(state);
}

void PushToCodexApp::render(Print& out, const DeviceState& state) {
  out.println("=== Push to Codex ===");
  out.print("Draft: ");
  out.println(draft_.length() > 0 ? draft_ : "(empty)");
  out.print("PTT: ");
  switch (state.ptt_state) {
    case PushToTalkState::Idle:
      out.println("idle");
      break;
    case PushToTalkState::Armed:
      out.println("armed");
      break;
    case PushToTalkState::Recording:
      out.println("recording");
      break;
    case PushToTalkState::Ready:
      out.println("ready");
      break;
    case PushToTalkState::Error:
      out.println("error");
      break;
  }
  out.print("Samples: ");
  out.print(state.ptt_samples_captured);
  out.print("/");
  out.println(state.ptt_sample_limit);
  out.print("Peak: ");
  out.println(state.ptt_peak_amplitude);
  out.print("Usage: ");
  if (state.codex_usage_percent >= 0) {
    out.print(state.codex_usage_percent);
    out.println("%");
  } else {
    out.println("unknown");
  }
  out.print("Detail: ");
  out.println(state.ptt_detail_line);
  out.print("Net: ");
  out.println(state.network_status_line);
  out.print("Codex state: ");
  switch (state.codex_state) {
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
  out.print("Workspace: ");
  out.println(state.codex_workspace_path);
  out.print("Branch: ");
  out.println(state.codex_branch.length() > 0 ? state.codex_branch : "(none)");
  out.print("Thread: ");
  out.println(state.codex_thread_id.length() > 0 ? state.codex_thread_id : "(none)");
  out.print("Bridge: ");
  out.println(state.bridge_status_line.length() > 0 ? state.bridge_status_line : "(idle)");
  if (state.codex_stream_line.length() > 0) {
    out.print("Last: ");
    out.println(state.codex_stream_line);
  }
  out.print("Approval: ");
  out.println(state.approval_pending ? "pending" : "clear");
  if (state.approval_pending && state.approval_detail_line.length() > 0) {
    out.print("Approval detail: ");
    out.println(state.approval_detail_line);
  }
  out.print("Status: ");
  out.println(state.status_line);
  out.println("Hold SPACE to record, release to finalize.");
  print_common_footer(out);
}

const char* PagerApp::title() const { return "Codex Pager"; }

void PagerApp::onEnter(DeviceState& state) {
  state.status_line = "Pager mode active";
}

void PagerApp::onExit(DeviceState& state) {
  (void)state;
}

void PagerApp::onCommand(const String& command, DeviceState& state) {
  (void)command;
  state.status_line = "Pager inbox and session detail will arrive next";
}

void PagerApp::tick(DeviceState& state) {
  (void)state;
}

void PagerApp::render(Print& out, const DeviceState& state) {
  out.println("=== Codex Pager ===");
  out.print("Active app: ");
  out.println(app_label(state.active_app));
  out.print("Net: ");
  out.println(state.network_status_line);
  out.print("Usage: ");
  if (state.codex_usage_percent >= 0) {
    out.print(state.codex_usage_percent);
    out.print("%");
    if (state.codex_usage_window_minutes > 0) {
      out.print(" window ");
      out.print(state.codex_usage_window_minutes);
      out.print("m");
    }
    if (state.codex_usage_resets_at > 0) {
      out.print(" reset ");
      out.print(state.codex_usage_resets_at);
    }
    out.println();
  } else {
    out.println("unknown");
  }
  if (state.codex_usage_detail_line.length() > 0) {
    out.print("Usage detail: ");
    out.println(state.codex_usage_detail_line);
  }
  out.print("Workspace: ");
  out.println(state.codex_workspace_path);
  out.print("Branch: ");
  out.println(state.codex_branch.length() > 0 ? state.codex_branch : "(none)");
  out.print("Thread: ");
  out.println(state.codex_thread_id.length() > 0 ? state.codex_thread_id : "(none)");
  out.print("Approval: ");
  out.println(state.approval_pending ? "pending" : "clear");
  out.print("Bridge: ");
  out.println(state.bridge_status_line.length() > 0 ? state.bridge_status_line : "(idle)");
  if (state.codex_stream_line.length() > 0) {
    out.print("Last stream: ");
    out.println(state.codex_stream_line);
  }
  if (state.approval_pending) {
    out.print("Approval request: ");
    out.println(state.approval_title.length() > 0 ? state.approval_title : "(untitled)");
    if (state.approval_detail_line.length() > 0) {
      out.print("Detail: ");
      out.println(state.approval_detail_line);
    }
    out.println("Keys: Enter=approve, Del=reject");
  }
  out.println("Recent activity:");
  const size_t log_count = state.activity_log_count;
  if (log_count == 0) {
    out.println("  (no events yet)");
  } else {
    for (size_t i = 0; i < log_count; ++i) {
      const String entry = activity_log_entry(state, i);
      out.print("  ");
      out.println(entry.length() > 0 ? entry : "(empty)");
    }
  }
  out.print("Status: ");
  out.println(state.status_line);
  print_common_footer(out);
}

const char* McpBridgeApp::title() const { return "Cardputer MCP Bridge"; }

void McpBridgeApp::onEnter(DeviceState& state) {
  state.status_line = "Local bridge mode active";
}

void McpBridgeApp::onExit(DeviceState& state) {
  (void)state;
}

void McpBridgeApp::onCommand(const String& command, DeviceState& state) {
  const String trimmed = trim_copy(command);
  if (trimmed.length() == 0) {
    return;
  }

  if (trimmed.startsWith("/notify ") || trimmed.startsWith("notify ")) {
    const String detail = trimmed.substring(trimmed.indexOf(' ') + 1);
    presentNotification("Notification", detail, state);
    if (bridge_ != nullptr) {
      bridge_->sendBridgeNotification("Notification", detail);
    }
    return;
  }

  if (trimmed.startsWith("/confirm ") || trimmed.startsWith("confirm ")) {
    const String detail = trimmed.substring(trimmed.indexOf(' ') + 1);
    presentConfirmation(detail, state);
    if (bridge_ != nullptr) {
      bridge_->sendBridgeConfirmation("Confirmation", detail);
    }
    return;
  }

  if (trimmed.startsWith("/ask ") || trimmed.startsWith("ask ")) {
    const String payload = trimmed.substring(trimmed.indexOf(' ') + 1);
    std::array<String, 5> parts{};
    size_t part_count = 0;
    size_t start = 0;
    while (start <= payload.length() && part_count < parts.size()) {
      const int separator = payload.indexOf('|', start);
      const String segment = trim_copy(separator < 0 ? payload.substring(start) : payload.substring(start, separator));
      parts[part_count++] = segment;
      if (separator < 0) {
        break;
      }
      start = static_cast<size_t>(separator + 1);
    }

    const String title = part_count > 0 && parts[0].length() > 0 ? parts[0] : "Question";
    const String detail = part_count > 1 ? parts[1] : "";
    const String opt1 = part_count > 2 ? parts[2] : "Yes";
    const String opt2 = part_count > 3 ? parts[3] : "No";
    const String opt3 = part_count > 4 ? parts[4] : "";
    presentQuestion(title, detail, opt1, opt2, opt3, state);
    if (bridge_ != nullptr) {
      std::array<String, 3> options{opt1, opt2, opt3};
      bridge_->sendBridgeQuestion(title, detail, options, opt3.length() > 0 ? 3 : (opt2.length() > 0 ? 2 : 1));
    }
    return;
  }

  if (trimmed.startsWith("/bridge ")) {
    const String payload = trimmed.substring(8);
    if (payload.startsWith("select ")) {
      const int index = constrain(payload.substring(7).toInt() - 1, 0, static_cast<int>(state.bridge_prompt_option_count > 0 ? state.bridge_prompt_option_count - 1 : 0));
      state.bridge_prompt_selected_index = static_cast<size_t>(index);
      state.status_line = String("Bridge option selected: ") +
                          (state.bridge_prompt_option_count > 0 ? state.bridge_prompt_options[state.bridge_prompt_selected_index] : String(index + 1));
      append_activity_event(state, state.status_line);
      return;
    }

    if (payload == "accept") {
      respondToPrompt(true, state);
      return;
    }

    if (payload == "reject") {
      respondToPrompt(false, state);
      return;
    }

    if (payload == "next" && state.bridge_prompt_option_count > 0) {
      state.bridge_prompt_selected_index = (state.bridge_prompt_selected_index + 1) % state.bridge_prompt_option_count;
      state.status_line = String("Bridge option selected: ") + state.bridge_prompt_options[state.bridge_prompt_selected_index];
      append_activity_event(state, state.status_line);
      return;
    }

    if (payload == "prev" && state.bridge_prompt_option_count > 0) {
      state.bridge_prompt_selected_index = (state.bridge_prompt_selected_index + state.bridge_prompt_option_count - 1) % state.bridge_prompt_option_count;
      state.status_line = String("Bridge option selected: ") + state.bridge_prompt_options[state.bridge_prompt_selected_index];
      append_activity_event(state, state.status_line);
      return;
    }
  }

  state.status_line = "Notification, ask, and confirm flows are ready";
}

void McpBridgeApp::onSubmit(const String& command, DeviceState& state) {
  onCommand(command, state);
}

void McpBridgeApp::setBridge(MiddlewareLink* bridge) {
  bridge_ = bridge;
}

void McpBridgeApp::tick(DeviceState& state) {
  (void)state;
}

void McpBridgeApp::render(Print& out, const DeviceState& state) {
  out.println("=== Cardputer MCP Bridge ===");
  out.print("Bridge: ");
  out.println(state.bridge_status_line.length() > 0 ? state.bridge_status_line : "(idle)");
  out.print("Prompt: ");
  switch (state.bridge_prompt_kind) {
    case BridgePromptKind::None:
      out.println("none");
      break;
    case BridgePromptKind::Notification:
      out.println("notification");
      break;
    case BridgePromptKind::Question:
      out.println("question");
      break;
    case BridgePromptKind::Confirmation:
      out.println("confirmation");
      break;
  }
  if (state.bridge_prompt_title.length() > 0) {
    out.print("Title: ");
    out.println(state.bridge_prompt_title);
  }
  if (state.bridge_prompt_detail.length() > 0) {
    out.print("Detail: ");
    out.println(state.bridge_prompt_detail);
  }
  if (state.bridge_prompt_option_count > 0) {
    out.println("Options:");
    for (size_t i = 0; i < state.bridge_prompt_option_count; ++i) {
      out.print(i == state.bridge_prompt_selected_index ? " > " : "   ");
      out.print(i + 1);
      out.print(". ");
      out.println(state.bridge_prompt_options[i]);
    }
    out.println("Keys: Enter=accept, Del=reject, /bridge select <n>");
  }
  out.print("Pending: ");
  out.println(state.bridge_prompt_pending ? "yes" : "no");
  out.print("Status: ");
  out.println(state.status_line);
  print_common_footer(out);
}

void McpBridgeApp::presentNotification(const String& title, const String& detail, DeviceState& state) {
  clear_bridge_prompt(state);
  state.bridge_prompt_kind = BridgePromptKind::Notification;
  state.bridge_prompt_title = title;
  state.bridge_prompt_detail = detail;
  state.bridge_status_line = title;
  state.status_line = detail.length() > 0 ? detail : title;
  append_activity_event(state, String("Bridge notification: ") + title);
}

void McpBridgeApp::presentQuestion(const String& title, const String& detail, const String& option1, const String& option2, const String& option3, DeviceState& state) {
  clear_bridge_prompt(state);
  state.bridge_prompt_kind = BridgePromptKind::Question;
  state.bridge_prompt_pending = true;
  state.bridge_prompt_title = title;
  state.bridge_prompt_detail = detail;
  std::array<String, 3> options{option1, option2, option3};
  const size_t count = option3.length() > 0 ? 3 : (option2.length() > 0 ? 2 : 1);
  set_bridge_options(state, options, count);
  state.bridge_status_line = "Bridge question pending";
  state.status_line = detail.length() > 0 ? detail : title;
  append_activity_event(state, String("Bridge question: ") + title);
}

void McpBridgeApp::presentConfirmation(const String& detail, DeviceState& state) {
  clear_bridge_prompt(state);
  state.bridge_prompt_kind = BridgePromptKind::Confirmation;
  state.bridge_prompt_pending = true;
  state.bridge_prompt_title = "Confirmation";
  state.bridge_prompt_detail = detail;
  std::array<String, 3> options{"Accept", "Reject", ""};
  set_bridge_options(state, options, 2);
  state.bridge_status_line = "Bridge confirmation pending";
  state.status_line = detail.length() > 0 ? detail : "Confirmation pending";
  append_activity_event(state, String("Bridge confirmation: ") + detail);
}

void McpBridgeApp::respondToPrompt(bool accepted, DeviceState& state) {
  if (!state.bridge_prompt_pending) {
    state.status_line = accepted ? "No bridge prompt pending to accept" : "No bridge prompt pending to reject";
    append_activity_event(state, state.status_line);
    return;
  }

  const size_t selected_index = state.bridge_prompt_selected_index;
  const String selected_text = selected_index < state.bridge_prompt_option_count ? state.bridge_prompt_options[selected_index] : "";
  if (bridge_ != nullptr) {
    bridge_->sendBridgeResponse(accepted, selected_index, selected_text);
  }
  clear_bridge_prompt(state);
  state.bridge_status_line = accepted ? "Bridge prompt accepted" : "Bridge prompt rejected";
  state.status_line = accepted ? "Bridge prompt accepted" : "Bridge prompt rejected";
  append_activity_event(state, state.status_line);
}

void PushToCodexApp::beginRecording(DeviceState& state) {
  if (recording_) {
    return;
  }

  if (!mic_started_) {
    state.ptt_state = PushToTalkState::Error;
    state.ptt_detail_line = "Microphone is not ready";
    state.status_line = "Microphone is not ready";
    return;
  }

  captured_sample_count_ = 0;
  peak_amplitude_ = 0;
  recording_ = true;
  state.ptt_state = PushToTalkState::Recording;
  state.ptt_samples_captured = 0;
  state.ptt_peak_amplitude = 0;
  state.ptt_detail_line = "Recording voice prompt...";
  state.status_line = "Recording voice prompt...";
  append_activity_event(state, "Recording voice prompt");
}

void PushToCodexApp::finishRecording(DeviceState& state) {
  if (!recording_) {
    if (captured_sample_count_ > 0) {
      state.ptt_state = PushToTalkState::Ready;
      state.ptt_detail_line = "Voice prompt ready for middleware";
      state.status_line = "Voice prompt ready for middleware";
      append_activity_event(state, "Voice prompt ready for middleware");
    } else if (state.ptt_state != PushToTalkState::Error) {
      state.ptt_state = PushToTalkState::Armed;
      state.ptt_detail_line = "Hold SPACE to record a voice prompt";
      state.status_line = "Hold SPACE to record a voice prompt";
    }
    return;
  }

  recording_ = false;
  if (captured_sample_count_ > 0) {
    if (bridge_ != nullptr) {
      bridge_->sendVoicePromptReady(state.ptt_sample_rate_hz, captured_sample_count_, peak_amplitude_);
    }
    emitVoicePromptEnvelope(state);
    state.ptt_state = PushToTalkState::Ready;
    state.ptt_detail_line = "Voice prompt ready for middleware";
    state.status_line = "Voice prompt ready for middleware";
    append_activity_event(state, "Voice prompt ready for middleware");
  } else {
    state.ptt_state = PushToTalkState::Armed;
    state.ptt_detail_line = "No audio captured";
    state.status_line = "No audio captured";
    append_activity_event(state, "No audio captured");
  }
}

void PushToCodexApp::appendChunk(const int16_t* data, size_t length, DeviceState& state) {
  const size_t remaining = kMaxSamples - captured_sample_count_;
  const size_t actual = remaining < length ? remaining : length;
  if (actual == 0) {
    state.ptt_state = PushToTalkState::Ready;
    state.ptt_detail_line = "Capture buffer full";
    state.status_line = "Capture buffer full";
    append_activity_event(state, "Capture buffer full");
    recording_ = false;
    return;
  }

  for (size_t i = 0; i < actual; ++i) {
    captured_samples_[captured_sample_count_ + i] = data[i];
    const int amplitude = abs(static_cast<int>(data[i]));
    if (amplitude > peak_amplitude_) {
      peak_amplitude_ = amplitude;
    }
  }
  captured_sample_count_ += actual;
  if (bridge_ != nullptr) {
    bridge_->sendAudioChunk(chunk_index_++, data, actual, state.ptt_sample_rate_hz);
  }
  state.ptt_samples_captured = captured_sample_count_;
  state.ptt_peak_amplitude = peak_amplitude_;

  if (captured_sample_count_ >= kMaxSamples) {
    state.ptt_state = PushToTalkState::Ready;
    state.ptt_detail_line = "Capture buffer full";
    state.status_line = "Capture buffer full";
    append_activity_event(state, "Capture buffer full");
    recording_ = false;
  }
}

void PushToCodexApp::updatePttState(DeviceState& state) {
  if (recording_) {
    state.ptt_state = PushToTalkState::Recording;
    const size_t duration_ms = captured_sample_count_ * 1000 / 16000;
    state.ptt_detail_line = String("Recording ") + String(duration_ms) + " ms";
    state.status_line = "Recording voice prompt...";
  } else if (captured_sample_count_ > 0) {
    state.ptt_state = PushToTalkState::Ready;
    const size_t duration_ms = captured_sample_count_ * 1000 / 16000;
    state.ptt_detail_line = String("Captured ") + String(duration_ms) + " ms";
    state.ptt_peak_amplitude = peak_amplitude_;
  } else if (state.ptt_state != PushToTalkState::Error) {
    state.ptt_state = PushToTalkState::Armed;
    state.ptt_detail_line = "Hold SPACE to record a voice prompt";
  }
}

void PushToCodexApp::emitVoicePromptEnvelope(const DeviceState& state) const {
  Serial.print("{\"type\":\"voice_prompt_ready\",\"sample_rate_hz\":");
  Serial.print(state.ptt_sample_rate_hz);
  Serial.print(",\"sample_count\":");
  Serial.print(state.ptt_samples_captured);
  Serial.print(",\"duration_ms\":");
  Serial.print((state.ptt_samples_captured * 1000UL) / state.ptt_sample_rate_hz);
  Serial.print(",\"peak_amplitude\":");
  Serial.print(state.ptt_peak_amplitude);
  Serial.println("}");
}
