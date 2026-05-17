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
    case AppId::Settings:
      return "Settings";
  }
  return "Unknown";
}

String trim_copy(String value) {
  value.trim();
  return value;
}

const char* codex_state_label(CodexState state) {
  switch (state) {
    case CodexState::Offline:
      return "offline";
    case CodexState::Idle:
      return "idle";
    case CodexState::Busy:
      return "busy";
    case CodexState::WaitingForApproval:
      return "waiting for approval";
  }
  return "unknown";
}

const char* ptt_state_label(PushToTalkState state) {
  switch (state) {
    case PushToTalkState::Idle:
      return "idle";
    case PushToTalkState::Armed:
      return "armed";
    case PushToTalkState::Recording:
      return "recording";
    case PushToTalkState::Ready:
      return "ready";
    case PushToTalkState::Error:
      return "error";
  }
  return "unknown";
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

String summarize_session_event(const PagerSessionEvent& event) {
  if (event.content.length() > 0) {
    return event.content;
  }
  if (event.type.length() > 0) {
    return event.type;
  }
  return "(event)";
}

String pager_screen_label(PagerScreen screen) {
  switch (screen) {
    case PagerScreen::Compose:
      return "COMPOSE";
    case PagerScreen::Inbox:
      return "INBOX";
    case PagerScreen::Detail:
      return "DETAIL";
  }
  return "PAGER";
}

bool pager_list_is_scrollable(const DeviceState& state) {
  return state.pager.session_count > 4;
}
}  // namespace

const char* BuddyApp::title() const { return "Codex Buddy"; }

void BuddyApp::onEnter(DeviceState& state) {
  state.ui_mode = UiMode::Home;
  state.status_line = "Buddy mode active";
}

void BuddyApp::onExit(DeviceState& state) {
  (void)state;
}

void BuddyApp::onCommand(const String& command, DeviceState& state) {
  (void)command;
  state.status_line = "Buddy view ready for live status";
}

void BuddyApp::onAction(UiAction action, DeviceState& state) {
  if (action == UiAction::Select) {
    state.status_line = "Open Pager to browse sessions";
    append_activity_event(state, state.status_line);
  }
}

void BuddyApp::tick(DeviceState& state) {
  (void)state;
}

void BuddyApp::render(Print& out, const DeviceState& state) {
  out.print("Wi-Fi   ");
  if (state.wifi_connected) {
    out.print(state.wifi_ssid.length() > 0 ? state.wifi_ssid : String("connected"));
    if (state.wifi_ip.length() > 0) {
      out.print(' ');
      out.print(state.wifi_ip);
    }
    out.println();
  } else {
    out.println("offline");
  }

  out.print("Bridge  ");
  out.println(state.bridge_status_line.length() > 0 ? state.bridge_status_line : String("idle"));

  out.print("Usage   ");
  if (state.codex_usage_percent >= 0) {
    out.print(state.codex_usage_percent);
    out.print('%');
    if (state.codex_usage_window_minutes > 0) {
      out.print(" / ");
      out.print(state.codex_usage_window_minutes);
      out.print('m');
    }
    out.println();
  } else {
    out.println("--");
  }

  out.println();

  out.print("Project ");
  out.println(state.codex_workspace_path.length() > 0 ? state.codex_workspace_path : String("(none)"));
  out.print("Branch  ");
  out.println(state.codex_branch.length() > 0 ? state.codex_branch : String("-"));
  out.print("Thread  ");
  out.println(state.codex_thread_id.length() > 0 ? state.codex_thread_id : String("-"));

  if (state.codex_stream_line.length() > 0) {
    out.print("> ");
    out.println(state.codex_stream_line);
  }

  if (state.approval_pending) {
    out.println();
    out.print("! ");
    out.println(state.approval_title.length() > 0 ? state.approval_title : String("Approval needed"));
    if (state.approval_detail_line.length() > 0) {
      out.print("  ");
      out.println(state.approval_detail_line);
    }
  }
}

const char* PushToCodexApp::title() const { return "Push to Codex"; }

void PushToCodexApp::onEnter(DeviceState& state) {
  draft_ = "";
  captured_sample_count_ = 0;
  chunk_index_ = 0;
  peak_amplitude_ = 0;
  recording_ = false;
  mic_started_ = false;
  state.ui_mode = UiMode::Input;
  state.ptt_state = PushToTalkState::Armed;
  state.ptt_samples_captured = 0;
  state.ptt_sample_limit = kMaxSamples;
  state.ptt_sample_rate_hz = 16000;
  state.ptt_peak_amplitude = 0;
  state.ptt_detail_line = "Tap SPACE for a space, hold SPACE to record";
  state.status_line = "Tap SPACE for a space, hold SPACE to record";
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
  onTextInput(command, false, state);
}

void PushToCodexApp::onTextInput(const String& text, bool backspace, DeviceState& state) {
  if (backspace) {
    if (draft_.length() > 0) {
      draft_.remove(draft_.length() - 1);
    }
  }
  if (text.length() > 0) {
    draft_ += text;
  }
  state.status_line = draft_.length() > 0 ? "Prompt staged for delivery" : "Type a prompt";
}

void PushToCodexApp::onSubmit(const String& command, DeviceState& state) {
  const String prompt = draft_.length() > 0 ? draft_ : command;
  if (prompt.length() == 0) {
    state.status_line = "Prompt is empty";
    append_activity_event(state, state.status_line);
    return;
  }
  draft_ = prompt;
  state.status_line = "Text prompt staged for middleware bridge";
  if (bridge_ != nullptr && bridge_->sendTextPrompt(prompt)) {
    state.status_line = "Text prompt sent to middleware";
    append_activity_event(state, "Text prompt sent to middleware");
    draft_ = "";
  }
}

void PushToCodexApp::onAction(UiAction action, DeviceState& state) {
  if (action == UiAction::Select && draft_.length() > 0) {
    onSubmit(draft_, state);
  } else if (action == UiAction::Back && draft_.length() == 0) {
    state.status_line = "Push prompt cleared";
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
  out.print("Draft: ");
  out.println(draft_.length() > 0 ? draft_ : String("(empty)"));

  out.print("PTT   ");
  out.println(ptt_state_label(state.ptt_state));

  out.print("Buf   ");
  out.print(state.ptt_samples_captured);
  out.print(" / ");
  out.println(state.ptt_sample_limit);

  out.print("Peak  ");
  out.println(state.ptt_peak_amplitude);

  out.print("Codex ");
  out.println(codex_state_label(state.codex_state));

  out.print("Net   ");
  out.println(state.network_status_line.length() > 0 ? state.network_status_line : String("idle"));

  if (state.codex_stream_line.length() > 0) {
    out.print("> ");
    out.println(state.codex_stream_line);
  } else if (state.ptt_detail_line.length() > 0) {
    out.print("  ");
    out.println(state.ptt_detail_line);
  }

  out.println();
  out.println("Hold SPACE to record.");
}

const char* PagerApp::title() const { return "Codex Pager"; }

void PagerApp::onEnter(DeviceState& state) {
  state.ui_mode = UiMode::Menu;
  state.pager_screen = PagerScreen::Inbox;
  compose_draft_ = "";
  detail_note_ = "";
  detail_reply_mode_ = false;
  syncSelectionFromState(state);
  state.status_line = "Browse sessions or compose a prompt";
}

void PagerApp::onExit(DeviceState& state) {
  (void)state;
}

void PagerApp::onCommand(const String& command, DeviceState& state) {
  const String trimmed = trim_copy(command);
  if (trimmed.length() == 0) {
    return;
  }

  if (trimmed == "compose" || trimmed == "/compose") {
    showCompose(state, "Compose prompt and press Enter");
    return;
  }

  if (trimmed == "inbox" || trimmed == "/inbox") {
    showInbox(state, "Browse recent sessions");
    return;
  }

  if (trimmed == "detail" || trimmed == "/detail") {
    showDetail(state, "Inspect the selected session");
    return;
  }

  if (trimmed == "r" || trimmed == "reply" || trimmed == "/reply") {
    detail_reply_mode_ = true;
    showCompose(state, "Reply in the selected thread");
    return;
  }

  if (trimmed == "i" || trimmed == "interrupt" || trimmed == "/interrupt") {
    if (canInterrupt(state)) {
      state.status_line = "Interrupt is not wired yet";
    } else {
      state.status_line = "Interrupt unavailable";
    }
    append_activity_event(state, state.status_line);
    return;
  }

  if (trimmed == "next") {
    if (state.pager.session_count == 0) {
      state.status_line = "No sessions to browse";
      append_activity_event(state, state.status_line);
      return;
    }
    state.menu.selected_index = (state.menu.selected_index + 1) % state.pager.session_count;
    selectSession(state, state.menu.selected_index);
    return;
  }

  if (trimmed == "prev") {
    if (state.pager.session_count == 0) {
      state.status_line = "No sessions to browse";
      append_activity_event(state, state.status_line);
      return;
    }
    state.menu.selected_index = (state.menu.selected_index + state.pager.session_count - 1) % state.pager.session_count;
    selectSession(state, state.menu.selected_index);
    return;
  }

  if (state.pager_screen == PagerScreen::Inbox && (trimmed.startsWith("select ") || trimmed.startsWith("open "))) {
    const int index = trimmed.substring(trimmed.indexOf(' ') + 1).toInt();
    if (index > 0) {
      selectSession(state, static_cast<size_t>(index - 1));
      return;
    }
  }

  if (state.pager_screen == PagerScreen::Inbox) {
    const int index = trimmed.toInt();
    if (index > 0) {
      selectSession(state, static_cast<size_t>(index - 1));
      return;
    }
  }

  if (state.pager_screen == PagerScreen::Compose || detail_reply_mode_) {
    sendReply(state, trimmed);
    return;
  }

  if (state.pager_screen == PagerScreen::Detail) {
    sendReply(state, trimmed);
    return;
  }

  state.status_line = "Use compose, inbox, detail, reply, next, prev";
  append_activity_event(state, state.status_line);
}

void PagerApp::onSubmit(const String& command, DeviceState& state) {
  const String prompt = compose_draft_.length() > 0 ? compose_draft_ : trim_copy(command);
  if (state.pager_screen == PagerScreen::Compose || detail_reply_mode_ || state.pager_screen == PagerScreen::Detail) {
    sendReply(state, prompt);
    return;
  }

  onCommand(command, state);
}

void PagerApp::onTextInput(const String& text, bool backspace, DeviceState& state) {
  String& target = (state.pager_screen == PagerScreen::Compose || detail_reply_mode_) ? compose_draft_ : compose_draft_;
  if (backspace) {
    if (target.length() > 0) {
      target.remove(target.length() - 1);
    }
  }
  if (text.length() > 0) {
    target += text;
  }
  state.status_line = target.length() > 0 ? "Prompt staged" : "Compose prompt and press Enter";
}

void PagerApp::onAction(UiAction action, DeviceState& state) {
  if (action == UiAction::Up) {
    if (state.pager.session_count == 0) {
      return;
    }
    if (state.menu.selected_index == 0) {
      state.menu.selected_index = state.pager.session_count - 1;
    } else {
      state.menu.selected_index--;
    }
    if (state.menu.scroll_offset > state.menu.selected_index) {
      state.menu.scroll_offset = state.menu.selected_index;
    }
    if (state.pager_screen == PagerScreen::Detail) {
      selectSession(state, state.menu.selected_index);
    } else {
      state.pager_screen = PagerScreen::Inbox;
      state.status_line = "Browse sessions";
      append_activity_event(state, state.status_line);
    }
    return;
  }

  if (action == UiAction::Down) {
    if (state.pager.session_count == 0) {
      return;
    }
    state.menu.selected_index = (state.menu.selected_index + 1) % state.pager.session_count;
    if (state.menu.scroll_offset + 4 <= state.menu.selected_index) {
      state.menu.scroll_offset = state.menu.selected_index - 3;
    }
    if (state.pager_screen == PagerScreen::Detail) {
      selectSession(state, state.menu.selected_index);
    } else {
      state.pager_screen = PagerScreen::Inbox;
      state.status_line = "Browse sessions";
      append_activity_event(state, state.status_line);
    }
    return;
  }

  if (action == UiAction::Select) {
    if (state.pager_screen == PagerScreen::Inbox) {
      if (state.pager.session_count > 0) {
        selectSession(state, state.menu.selected_index);
      }
      return;
    }
    if (state.pager_screen == PagerScreen::Detail) {
      detail_reply_mode_ = true;
      showCompose(state, "Reply in the selected thread");
      return;
    }
    if (state.pager_screen == PagerScreen::Compose) {
      sendReply(state, compose_draft_);
      return;
    }
  }

  if (action == UiAction::Back) {
    if (state.pager_screen == PagerScreen::Detail) {
      showInbox(state, "Browse recent sessions");
      return;
    }
    if (state.pager_screen == PagerScreen::Compose) {
      showInbox(state, "Browse recent sessions");
      detail_reply_mode_ = false;
      return;
    }
  }

  if (action == UiAction::Menu) {
    showCompose(state, "Compose prompt and press Enter");
  }
}

void PagerApp::tick(DeviceState& state) {
  (void)state;
}

void PagerApp::render(Print& out, const DeviceState& state) {
  out.print("[ ");
  out.print(pager_screen_label(state.pager_screen));
  out.print(" ]");
  if (state.pager.active_session_id.length() > 0) {
    out.print("  active: ");
    out.print(state.pager.active_session_id);
  }
  out.println();
  out.println();

  if (state.pager_screen == PagerScreen::Compose) {
    const PagerSessionSummary* session = selectedSession(state);
    out.print("To:  ");
    out.println(session != nullptr && session->thread_id.length() > 0 ? session->thread_id : String("(new thread)"));
    if (session != nullptr && session->branch.length() > 0) {
      out.print("On:  ");
      out.println(session->branch);
    }
    out.println();
    out.println("Draft:");
    out.println(compose_draft_.length() > 0 ? compose_draft_ : String("(empty)"));
  } else if (state.pager_screen == PagerScreen::Inbox) {
    if (state.pager.session_count == 0) {
      out.println("(no sessions yet)");
    } else {
      const size_t start = state.menu.scroll_offset < state.pager.session_count ? state.menu.scroll_offset : 0;
      const size_t end = min(state.pager.session_count, start + 4);
      for (size_t i = start; i < end; ++i) {
        const PagerSessionSummary& session = state.pager.sessions[i];
        out.print(i == selectedSessionIndex(state) ? "> " : "  ");
        out.print(static_cast<unsigned>(i + 1));
        out.print(". ");
        out.println(session.title.length() > 0 ? session.title : String("(untitled)"));
        out.print("    [");
        out.print(session.status.length() > 0 ? session.status : String("idle"));
        out.print("] ");
        out.println(session.last_event.length() > 0 ? session.last_event : String("-"));
      }
    }
  } else {
    const PagerSessionSummary* session = selectedSession(state);
    if (session == nullptr) {
      out.println("(no session selected)");
    } else {
      out.print("Title:  ");
      out.println(session->title.length() > 0 ? session->title : session->session_id);
      out.print("Thread: ");
      out.println(session->thread_id.length() > 0 ? session->thread_id : String("-"));
      out.print("Branch: ");
      out.println(session->branch.length() > 0 ? session->branch : String("-"));
      out.print("State  ");
      out.println(session->status.length() > 0 ? session->status : String("idle"));
      if (session->pending_approval_id.length() > 0) {
        out.print("! Approval: ");
        out.println(session->pending_approval_id);
      }
      out.println();
      if (session->event_count > 0) {
        const size_t limit = session->event_count > 3 ? 3 : session->event_count;
        for (size_t i = 0; i < limit; ++i) {
          out.print("  - ");
          out.println(summarize_session_event(session->events[i]));
        }
      }
    }
  }
}

void PagerApp::showCompose(DeviceState& state, const String& message) {
  state.pager_screen = PagerScreen::Compose;
  state.ui_mode = UiMode::Input;
  state.status_line = message;
  append_activity_event(state, message);
}

void PagerApp::showInbox(DeviceState& state, const String& message) {
  state.pager_screen = PagerScreen::Inbox;
  state.ui_mode = UiMode::Menu;
  detail_reply_mode_ = false;
  syncSelectionFromState(state);
  state.status_line = message;
  append_activity_event(state, message);
}

void PagerApp::showDetail(DeviceState& state, const String& message) {
  state.pager_screen = PagerScreen::Detail;
  state.ui_mode = UiMode::Menu;
  detail_reply_mode_ = false;
  syncSelectionFromState(state);
  state.status_line = message;
  append_activity_event(state, message);
}

void PagerApp::selectSession(DeviceState& state, size_t index) {
  syncSelectionFromState(state);
  if (state.pager.session_count == 0) {
    state.status_line = "No sessions available";
    append_activity_event(state, state.status_line);
    return;
  }

  if (index >= state.pager.session_count) {
    index = state.pager.session_count - 1;
  }

  state.menu.selected_index = index;
  const PagerSessionSummary& session = state.pager.sessions[state.menu.selected_index];
  state.pager.selected_session_id = session.session_id;
  detail_reply_mode_ = false;
  state.pager_screen = PagerScreen::Detail;
  state.status_line = String("Selected session ") + String(state.menu.selected_index + 1);
  append_activity_event(state, state.status_line);

  if (bridge_ != nullptr) {
    if (session.workspace_path.length() > 0) {
      bridge_->sendProjectSelect(session.workspace_path);
    }
    if (session.branch.length() > 0) {
      bridge_->sendBranchSelect(session.branch);
    }
    if (session.thread_id.length() > 0) {
      bridge_->sendThreadSelect(session.thread_id);
    }
    bridge_->sendStatusRequest();
  }
}

bool PagerApp::sendReply(DeviceState& state, const String& prompt) {
  const String trimmed = trim_copy(prompt);
  if (trimmed.length() == 0) {
    state.status_line = "Prompt is empty";
    append_activity_event(state, state.status_line);
    return false;
  }

  compose_draft_ = trimmed;
  const PagerSessionSummary* session = selectedSession(state);
  if (session != nullptr && session->session_id.length() > 0) {
    state.pager.selected_session_id = session->session_id;
  }

  if (bridge_ == nullptr || !bridge_->sendTextPrompt(trimmed)) {
    state.status_line = "Prompt not sent";
    append_activity_event(state, state.status_line);
    return false;
  }

  state.status_line = detail_reply_mode_ ? "Reply sent" : "Prompt sent";
  append_activity_event(state, state.status_line);
  detail_reply_mode_ = false;
  compose_draft_ = "";
  // If the user composed without picking a session first, bounce back to the Inbox
  // instead of showing an empty Detail card.
  state.pager_screen = (selectedSession(state) != nullptr) ? PagerScreen::Detail
                                                           : PagerScreen::Inbox;
  state.ui_mode = UiMode::Menu;
  return true;
}

void PagerApp::setBridge(MiddlewareLink* bridge) {
  bridge_ = bridge;
}

bool PagerApp::canInterrupt(const DeviceState& state) const {
  return state.pager.interrupt_supported;
}

size_t PagerApp::selectedSessionIndex(const DeviceState& state) const {
  if (state.pager.session_count == 0) {
    return 0;
  }

  if (state.menu.selected_index < state.pager.session_count) {
    return state.menu.selected_index;
  }

  if (state.pager.selected_session_id.length() > 0) {
    for (size_t i = 0; i < state.pager.session_count; ++i) {
      if (state.pager.sessions[i].session_id == state.pager.selected_session_id) {
        return i;
      }
    }
  }

  if (state.pager.active_session_id.length() > 0) {
    for (size_t i = 0; i < state.pager.session_count; ++i) {
      if (state.pager.sessions[i].session_id == state.pager.active_session_id) {
        return i;
      }
    }
  }

  return 0;
}

const PagerSessionSummary* PagerApp::selectedSession(const DeviceState& state) const {
  if (state.pager.session_count == 0) {
    return nullptr;
  }

  const size_t index = selectedSessionIndex(state);
  if (index >= state.pager.session_count) {
    return nullptr;
  }
  return &state.pager.sessions[index];
}

void PagerApp::syncSelectionFromState(DeviceState& state) {
  if (state.pager.session_count == 0) {
    state.menu.selected_index = 0;
    return;
  }

  if (state.menu.selected_index < state.pager.session_count) {
    return;
  }

  if (state.pager.selected_session_id.length() > 0) {
    for (size_t i = 0; i < state.pager.session_count; ++i) {
      if (state.pager.sessions[i].session_id == state.pager.selected_session_id) {
        state.menu.selected_index = i;
        return;
      }
    }
  }

  if (state.pager.active_session_id.length() > 0) {
    for (size_t i = 0; i < state.pager.session_count; ++i) {
      if (state.pager.sessions[i].session_id == state.pager.active_session_id) {
        state.menu.selected_index = i;
        return;
      }
    }
  }

  state.menu.selected_index = 0;
}

const char* McpBridgeApp::title() const { return "Cardputer MCP Bridge"; }

void McpBridgeApp::onEnter(DeviceState& state) {
  state.ui_mode = UiMode::Menu;
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

void McpBridgeApp::onAction(UiAction action, DeviceState& state) {
  if (!state.bridge_prompt_pending) {
    if (action == UiAction::Select) {
      state.status_line = "Waiting for bridge prompt";
    }
    return;
  }

  if (action == UiAction::Up && state.bridge_prompt_option_count > 0) {
    state.bridge_prompt_selected_index =
      state.bridge_prompt_selected_index == 0 ? state.bridge_prompt_option_count - 1 : state.bridge_prompt_selected_index - 1;
    state.menu.selected_index = state.bridge_prompt_selected_index;
    state.status_line = String("Bridge option selected: ") +
                        state.bridge_prompt_options[state.bridge_prompt_selected_index];
    append_activity_event(state, state.status_line);
    return;
  }

  if (action == UiAction::Down && state.bridge_prompt_option_count > 0) {
    state.bridge_prompt_selected_index = (state.bridge_prompt_selected_index + 1) % state.bridge_prompt_option_count;
    state.menu.selected_index = state.bridge_prompt_selected_index;
    state.status_line = String("Bridge option selected: ") +
                        state.bridge_prompt_options[state.bridge_prompt_selected_index];
    append_activity_event(state, state.status_line);
    return;
  }

  if (action == UiAction::Select) {
    respondToPrompt(true, state);
    return;
  }

  if (action == UiAction::Back) {
    respondToPrompt(false, state);
  }
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
  out.print("Bridge: ");
  out.println(state.bridge_status_line.length() > 0 ? state.bridge_status_line : String("idle"));
  out.println();

  switch (state.bridge_prompt_kind) {
    case BridgePromptKind::None:
      out.println("(no active prompt)");
      out.println();
      out.println("Try one of:");
      out.println("  /notify <text>");
      out.println("  /ask t|d|a|b|c");
      out.println("  /confirm <text>");
      return;
    case BridgePromptKind::Notification:
      out.println("[ NOTIFICATION ]");
      break;
    case BridgePromptKind::Question:
      out.println("[ QUESTION ]");
      break;
    case BridgePromptKind::Confirmation:
      out.println("[ CONFIRM ]");
      break;
  }

  if (state.bridge_prompt_title.length() > 0) {
    out.println(state.bridge_prompt_title);
  }
  if (state.bridge_prompt_detail.length() > 0) {
    out.println(state.bridge_prompt_detail);
  }

  if (state.bridge_prompt_option_count > 0) {
    out.println();
    for (size_t i = 0; i < state.bridge_prompt_option_count; ++i) {
      out.print(i == state.bridge_prompt_selected_index ? " > " : "   ");
      out.println(state.bridge_prompt_options[i]);
    }
  }
}

void McpBridgeApp::presentNotification(const String& title, const String& detail, DeviceState& state) {
  clear_bridge_prompt(state);
  state.bridge_prompt_kind = BridgePromptKind::Notification;
  state.bridge_prompt_title = title;
  state.bridge_prompt_detail = detail;
  state.bridge_status_line = title;
  state.status_line = detail.length() > 0 ? detail : title;
  state.ui_mode = UiMode::Modal;
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
  state.ui_mode = UiMode::BridgePrompt;
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
  state.ui_mode = UiMode::BridgePrompt;
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
  state.ui_mode = UiMode::Menu;
  append_activity_event(state, state.status_line);
}

const char* SettingsApp::title() const { return "Settings"; }

void SettingsApp::onEnter(DeviceState& state) {
  state.ui_mode = UiMode::Menu;
  selected_index_ = state.menu.selected_index < kItemCount ? state.menu.selected_index : 0;
  state.menu.selected_index = selected_index_;
  state.status_line = "Settings ready";
}

void SettingsApp::onExit(DeviceState& state) {
  (void)state;
}

void SettingsApp::onCommand(const String& command, DeviceState& state) {
  const String trimmed = trim_copy(command);
  if (trimmed == "/help") {
    state.status_line = "Use Fn+;/. Enter Del and Ctrl-M menu";
    append_activity_event(state, state.status_line);
  }
}

void SettingsApp::onAction(UiAction action, DeviceState& state) {
  if (action == UiAction::Up) {
    selected_index_ = selected_index_ == 0 ? kItemCount - 1 : selected_index_ - 1;
    state.menu.selected_index = selected_index_;
    return;
  }

  if (action == UiAction::Down) {
    selected_index_ = (selected_index_ + 1) % kItemCount;
    state.menu.selected_index = selected_index_;
    return;
  }

  if (action == UiAction::Select) {
    switch (selected_index_) {
      case 0:
        state.status_line = state.wifi_connected ? "Wi-Fi connected" : "Wi-Fi offline";
        break;
      case 1:
        state.status_line = state.bridge_status_line.length() > 0 ? state.bridge_status_line : "Bridge not configured";
        break;
      case 2:
        state.status_line = "Ctrl-M menu, Fn+;/. move, Enter select, Del back";
        break;
      case 3:
        state.status_line = state.firmware_name;
        break;
    }
    append_activity_event(state, state.status_line);
  }
}

void SettingsApp::tick(DeviceState& state) {
  (void)state;
}

void SettingsApp::render(Print& out, const DeviceState& state) {
  const char* items[] = {"Wi-Fi", "Bridge", "Keymap", "About"};
  for (size_t i = 0; i < kItemCount; ++i) {
    out.print(i == selected_index_ ? " > " : "   ");
    out.println(items[i]);
  }
  out.println();
  out.println("Detail");
  switch (selected_index_) {
    case 0:
      if (state.wifi_connected) {
        out.print(" SSID ");
        out.println(state.wifi_ssid.length() > 0 ? state.wifi_ssid : String("(none)"));
        out.print(" IP   ");
        out.println(state.wifi_ip.length() > 0 ? state.wifi_ip : String("-"));
      } else {
        out.print(' ');
        out.println(state.network_status_line.length() > 0 ? state.network_status_line : String("offline"));
      }
      break;
    case 1:
      out.print(' ');
      out.println(state.bridge_status_line.length() > 0 ? state.bridge_status_line : String("(idle)"));
      break;
    case 2:
      out.println(" Ctrl-M menu");
      out.println(" Fn+;/. move selection");
      out.println(" Ent  select / accept");
      out.println(" Del  back / reject");
      out.println(" SPC  push-to-talk");
      break;
    case 3:
      out.print(' ');
      out.println(state.firmware_name);
      break;
  }
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
      state.ptt_detail_line = "Tap SPACE for a space, hold SPACE to record";
      state.status_line = "Tap SPACE for a space, hold SPACE to record";
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
    state.ptt_detail_line = "Tap SPACE for a space, hold SPACE to record";
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
