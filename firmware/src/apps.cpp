#include "apps.h"

#include <array>
#include <M5Cardputer.h>

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
  (void)command;
  state.status_line = "Notification, ask, and confirm flows will attach here";
}

void McpBridgeApp::tick(DeviceState& state) {
  (void)state;
}

void McpBridgeApp::render(Print& out, const DeviceState& state) {
  out.println("=== Cardputer MCP Bridge ===");
  out.println("Planned tools: notify, ask, confirm.");
  if (state.approval_pending) {
    out.print("Approval pending: ");
    out.println(state.approval_title.length() > 0 ? state.approval_title : "(untitled)");
  }
  out.print("Status: ");
  out.println(state.status_line);
  print_common_footer(out);
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
