#include "middleware_link.h"

#include <memory>
#include <mbedtls/base64.h>
#include <ESPmDNS.h>

#include "device_config.h"

MiddlewareLink* MiddlewareLink::instance_ = nullptr;

void MiddlewareLink::begin(DeviceState& state) {
  state_ = &state;
  instance_ = this;
  client_.onEvent(handleWebSocketEvent);
  if (CARDPUTER_MIDDLEWARE_HOST[0] != '\0') {
    configure(
      CARDPUTER_MIDDLEWARE_HOST,
      static_cast<uint16_t>(CARDPUTER_MIDDLEWARE_PORT),
      CARDPUTER_MIDDLEWARE_PATH,
      CARDPUTER_MIDDLEWARE_TOKEN
    );
  }
}

void MiddlewareLink::configure(const String& host, uint16_t port, const String& path, const String& auth_token) {
  host_ = host;
  port_ = port;
  path_ = path;
  configured_ = host_.length() > 0 && port_ > 0;
  auth_token_ = auth_token;
  started_ = false;
}

bool MiddlewareLink::isConfigured() const {
  return configured_;
}

bool MiddlewareLink::isConnected() {
  return client_.isConnected();
}

void MiddlewareLink::tick(DeviceState& state) {
  state_ = &state;
  if (!configured_) {
    state.bridge_status_line = "Middleware bridge disabled";
    return;
  }

  if (!ensureConnection(state)) {
    client_.loop();
    return;
  }

  client_.loop();
}

bool MiddlewareLink::sendTextPrompt(const String& text) {
  return sendCardputerMessage(buildEnvelope(nextMessageId(), "text_prompt", String("{\"text\":\"") + escapeJson(text) + "\"}"));
}

bool MiddlewareLink::sendProjectSelect(const String& workspace_path) {
  return sendCardputerMessage(buildEnvelope(nextMessageId(), "project_select", String("{\"workspace_path\":\"") + escapeJson(workspace_path) + "\"}"));
}

bool MiddlewareLink::sendBranchSelect(const String& branch) {
  return sendCardputerMessage(buildEnvelope(nextMessageId(), "branch_select", String("{\"branch\":\"") + escapeJson(branch) + "\"}"));
}

bool MiddlewareLink::sendThreadSelect(const String& thread_id) {
  return sendCardputerMessage(buildEnvelope(nextMessageId(), "thread_select", String("{\"thread_id\":\"") + escapeJson(thread_id) + "\"}"));
}

bool MiddlewareLink::sendAudioChunk(size_t chunk_id, const int16_t* samples, size_t sample_count, uint32_t sample_rate_hz) {
  if (samples == nullptr || sample_count == 0) {
    return false;
  }

  const size_t byte_count = sample_count * sizeof(int16_t);
  const size_t encoded_capacity = 4 * ((byte_count + 2) / 3) + 1;
  std::unique_ptr<uint8_t[]> encoded(new uint8_t[encoded_capacity]);
  size_t encoded_length = 0;
  const int rc = mbedtls_base64_encode(
    encoded.get(),
    encoded_capacity,
    &encoded_length,
    reinterpret_cast<const unsigned char*>(samples),
    byte_count
  );
  if (rc != 0) {
    return false;
  }

  encoded[encoded_length] = '\0';
  String pcm_b64(reinterpret_cast<const char*>(encoded.get()));
  String payload = String("{\"chunk_id\":") + String(chunk_id) +
                   String(",\"pcm_b64\":\"") + pcm_b64 +
                   String("\",\"sample_rate_hz\":") + String(sample_rate_hz) + "}";
  return sendCardputerMessage(buildEnvelope(nextMessageId(), "audio_chunk", payload));
}

bool MiddlewareLink::sendVoicePromptReady(uint32_t sample_rate_hz, size_t sample_count, int peak_amplitude) {
  String payload = String("{\"sample_rate_hz\":") + String(sample_rate_hz) +
                   String(",\"sample_count\":") + String(sample_count) +
                   String(",\"peak_amplitude\":") + String(peak_amplitude) + "}";
  return sendCardputerMessage(buildEnvelope(nextMessageId(), "voice_prompt_ready", payload));
}

bool MiddlewareLink::sendApprovalResponse(bool approved) {
  String payload = String("{\"approved\":") + (approved ? "true" : "false") + "}";
  return sendCardputerMessage(buildEnvelope(nextMessageId(), "approval_response", payload));
}

bool MiddlewareLink::sendBridgeNotification(const String& title, const String& detail) {
  String payload = String("{\"title\":\"") + escapeJson(title) + String("\",\"detail\":\"") + escapeJson(detail) + "\"}";
  return sendCardputerMessage(buildEnvelope(nextMessageId(), "bridge_notification", payload));
}

bool MiddlewareLink::sendBridgeQuestion(const String& title, const String& detail, const std::array<String, 3>& options, size_t option_count) {
  String payload = String("{\"title\":\"") + escapeJson(title) + String("\",\"detail\":\"") + escapeJson(detail) + String("\",\"options\":[");
  for (size_t i = 0; i < option_count && i < options.size(); ++i) {
    if (i > 0) {
      payload += ',';
    }
    payload += String("\"") + escapeJson(options[i]) + "\"";
  }
  payload += "]}";
  return sendCardputerMessage(buildEnvelope(nextMessageId(), "bridge_question", payload));
}

bool MiddlewareLink::sendBridgeConfirmation(const String& title, const String& detail) {
  String payload = String("{\"title\":\"") + escapeJson(title) + String("\",\"detail\":\"") + escapeJson(detail) + "\"}";
  return sendCardputerMessage(buildEnvelope(nextMessageId(), "bridge_confirmation", payload));
}

bool MiddlewareLink::sendBridgeResponse(bool accepted, size_t selected_index, const String& note) {
  String payload = String("{\"accepted\":") + (accepted ? "true" : "false") +
                   String(",\"selected_index\":") + String(selected_index) +
                   String(",\"note\":\"") + escapeJson(note) + "\"}";
  return sendCardputerMessage(buildEnvelope(nextMessageId(), "bridge_response", payload));
}

bool MiddlewareLink::sendDisplaySnapshot(
  const String& screen_text,
  const String& status_line,
  const String& active_app,
  const String& input_line,
  const String& firmware_name,
  const String& network_status_line
) {
  String payload = String("{\"screen_text\":\"") + escapeJson(screen_text) +
                   String("\",\"status_line\":\"") + escapeJson(status_line) +
                   String("\",\"active_app\":\"") + escapeJson(active_app) +
                   String("\",\"input_line\":\"") + escapeJson(input_line) +
                   String("\",\"firmware_name\":\"") + escapeJson(firmware_name) +
                   String("\",\"network_status_line\":\"") + escapeJson(network_status_line) + "\"}";
  return sendCardputerMessage(buildEnvelope(nextMessageId(), "display_snapshot", payload));
}

bool MiddlewareLink::sendStatusRequest() {
  return sendCardputerMessage(buildEnvelope(nextMessageId(), "status_request", "{}"));
}

void MiddlewareLink::handleWebSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  if (instance_ != nullptr) {
    instance_->onWebSocketEvent(type, payload, length);
  }
}

void MiddlewareLink::onWebSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  if (state_ == nullptr) {
    return;
  }

  switch (type) {
    case WStype_CONNECTED:
      state_->bridge_status_line = "Middleware connected";
      append_activity_event(*state_, "Middleware bridge connected");
      sendStatusRequest();
      break;
    case WStype_DISCONNECTED:
      state_->bridge_status_line = "Middleware disconnected";
      append_activity_event(*state_, "Middleware bridge disconnected");
      break;
    case WStype_TEXT: {
      JsonDocument doc;
      const DeserializationError error = deserializeJson(doc, payload, length);
      if (error) {
        state_->bridge_status_line = String("Bridge JSON error: ") + error.c_str();
        append_activity_event(*state_, state_->bridge_status_line);
        return;
      }

      const String event_type = doc["type"] | "";
      const String message_id = doc["id"] | "";
      JsonObjectConst payload_variant = doc["payload"];
      if (payload_variant.isNull()) {
        state_->bridge_status_line = "Bridge payload malformed";
        append_activity_event(*state_, state_->bridge_status_line);
        return;
      }

      if (event_type == "ack") {
        const bool ok = payload_variant["ok"] | false;
        state_->bridge_status_line = String("Ack ") + message_id + (ok ? " ok" : " failed");
        append_activity_event(*state_, state_->bridge_status_line);
        return;
      }

      applyIncomingEvent(*state_, event_type, payload_variant);
      break;
    }
    default:
      break;
  }
}

bool MiddlewareLink::sendCardputerMessage(const String& message) {
  if (!client_.isConnected()) {
    return false;
  }
  String mutable_message = message;
  return client_.sendTXT(mutable_message);
}

bool MiddlewareLink::ensureConnection(DeviceState& state) {
  if (!configured_ || started_) {
    return client_.isConnected();
  }

  client_.begin(host_.c_str(), port_, path_.c_str());
  client_.setReconnectInterval(5000);
  client_.enableHeartbeat(15000, 3000, 2);
  state.bridge_status_line = String("Connecting bridge to ws://") + host_ + ":" + String(port_) + path_;
  append_activity_event(state, state.bridge_status_line);
  started_ = true;
  return client_.isConnected();
}

void MiddlewareLink::applyIncomingEvent(DeviceState& state, const String& event_type, JsonObjectConst payload) {
  if (payload.containsKey("state_epoch")) {
    uint32_t incoming_epoch = payload["state_epoch"];
    if (incoming_epoch <= state.state_epoch && incoming_epoch != 0) {
      return;
    }
    state.state_epoch = incoming_epoch;
  }

  const String kind = payload["kind"] | "";
  const String content = payload["content"] | "";

  if (event_type == "codex_status") {
    if (kind == "session_status") {
      if (payload["active_session_id"].is<const char*>()) {
        state.pager.active_session_id = payload["active_session_id"].as<const char*>();
      }
      if (payload["interrupt_supported"].is<bool>()) {
        state.pager.interrupt_supported = payload["interrupt_supported"].as<bool>();
      }
      if (payload["workspace_path"].is<const char*>()) {
        state.codex_workspace_path = payload["workspace_path"].as<const char*>();
      }
      if (payload["branch"].is<const char*>()) {
        state.codex_branch = payload["branch"].as<const char*>();
      }
      if (payload["thread_id"].is<const char*>()) {
        state.codex_thread_id = payload["thread_id"].as<const char*>();
      }
      if (payload["status"].is<const char*>()) {
        const String session_status = payload["status"].as<const char*>();
        state.codex_state = session_status == "approval" ? CodexState::WaitingForApproval
                           : session_status == "running"   ? CodexState::Busy
                           : session_status == "error"     ? CodexState::Offline
                                                           : CodexState::Idle;
      }
      if (payload["last_event"].is<const char*>()) {
        state.codex_stream_line = payload["last_event"].as<const char*>();
      }
      if (payload["approval_id"].is<const char*>()) {
        const String approval_id = payload["approval_id"].as<const char*>();
        state.approval_pending = approval_id.length() > 0;
        state.approval_id = approval_id;
      }
      if (payload["approval_title"].is<const char*>()) {
        state.approval_title = payload["approval_title"].as<const char*>();
      }
      if (payload["approval_detail"].is<const char*>()) {
        state.approval_detail_line = payload["approval_detail"].as<const char*>();
      }
      state.pager.session_count = 0;
      state.pager.selected_session_id = state.pager.active_session_id;
      JsonArrayConst sessions = payload["sessions"];
      if (!sessions.isNull()) {
        size_t i = 0;
        for (JsonVariantConst session_variant : sessions) {
          if (i >= state.pager.sessions.size()) {
            break;
          }
          if (!session_variant.is<JsonObjectConst>()) {
            continue;
          }
          JsonObjectConst session = session_variant.as<JsonObjectConst>();
          PagerSessionSummary& view = state.pager.sessions[i];
          view.session_id = session["session_id"] | "";
          view.thread_id = session["thread_id"] | "";
          view.workspace_path = session["workspace_path"] | "";
          view.branch = session["branch"] | "";
          view.title = session["title"] | "";
          view.status = session["status"] | "";
          view.last_event = session["last_event"] | "";
          view.pending_approval_id = session["pending_approval_id"] | "";
          view.event_count = 0;
          JsonArrayConst events = session["events"];
          if (!events.isNull()) {
            size_t j = 0;
            for (JsonVariantConst event_variant : events) {
              if (j >= view.events.size()) {
                break;
              }
              if (!event_variant.is<JsonObjectConst>()) {
                continue;
              }
              JsonObjectConst event = event_variant.as<JsonObjectConst>();
              view.events[j].type = event["type"] | "";
              JsonObjectConst event_payload = event["payload"];
              if (!event_payload.isNull()) {
                const String content = event_payload["content"] | event_payload["text"] | event_payload["message"] | "";
                if (content.length() > 0) {
                  view.events[j].content = content;
                } else {
                  view.events[j].content = event_payload["kind"] | "";
                }
              } else {
                view.events[j].content = "";
              }
              ++j;
            }
            view.event_count = j;
          }
          ++i;
        }
        state.pager.session_count = i;
      }
      state.status_line = content.length() > 0 ? content : "Session status updated";
      state.bridge_status_line = "Session synchronized";
      append_activity_event(state, state.status_line);
      return;
    }

    if (kind == "bridge_notification") {
      state.bridge_prompt_kind = BridgePromptKind::Notification;
      state.bridge_prompt_pending = false;
      state.bridge_prompt_title = payload["title"] | "Notification";
      if (state.bridge_prompt_title.length() == 0 && content.length() > 0) {
        state.bridge_prompt_title = content;
      }
      state.bridge_prompt_detail = payload["detail"] | "";
      if (state.bridge_prompt_detail.length() == 0 && content.length() > 0) {
        state.bridge_prompt_detail = content;
      }
      state.bridge_prompt_option_count = 0;
      state.bridge_prompt_selected_index = 0;
      state.bridge_status_line = state.bridge_prompt_title;
      state.status_line = state.bridge_prompt_detail.length() > 0 ? state.bridge_prompt_detail : state.bridge_prompt_title;
      append_activity_event(state, state.status_line);
      return;
    }

    if (kind == "bridge_question") {
      state.bridge_prompt_kind = BridgePromptKind::Question;
      state.bridge_prompt_pending = true;
      state.bridge_prompt_title = payload["title"] | "Question";
      if (state.bridge_prompt_title.length() == 0 && content.length() > 0) {
        state.bridge_prompt_title = content;
      }
      state.bridge_prompt_detail = payload["detail"] | "";
      state.bridge_prompt_selected_index = 0;
      state.bridge_prompt_option_count = 0;
      state.bridge_prompt_options[0] = "";
      state.bridge_prompt_options[1] = "";
      state.bridge_prompt_options[2] = "";
      JsonArrayConst options = payload["options"];
      if (!options.isNull()) {
        size_t i = 0;
        for (JsonVariantConst option : options) {
          if (i >= state.bridge_prompt_options.size()) {
            break;
          }
          state.bridge_prompt_options[i++] = option.is<const char*>() ? option.as<const char*>() : "";
        }
        state.bridge_prompt_option_count = i;
      }
      state.bridge_status_line = "Bridge question pending";
      state.status_line = state.bridge_prompt_detail.length() > 0 ? state.bridge_prompt_detail : state.bridge_prompt_title;
      append_activity_event(state, state.bridge_prompt_title);
      return;
    }

    if (kind == "bridge_confirmation") {
      state.bridge_prompt_kind = BridgePromptKind::Confirmation;
      state.bridge_prompt_pending = true;
      state.bridge_prompt_title = payload["title"] | "Confirmation";
      if (state.bridge_prompt_title.length() == 0 && content.length() > 0) {
        state.bridge_prompt_title = content;
      }
      state.bridge_prompt_detail = payload["detail"] | "";
      if (state.bridge_prompt_detail.length() == 0 && content.length() > 0) {
        state.bridge_prompt_detail = content;
      }
      state.bridge_prompt_selected_index = 0;
      state.bridge_prompt_options[0] = "Accept";
      state.bridge_prompt_options[1] = "Reject";
      state.bridge_prompt_options[2] = "";
      state.bridge_prompt_option_count = 2;
      state.bridge_status_line = "Bridge confirmation pending";
      state.status_line = state.bridge_prompt_detail.length() > 0 ? state.bridge_prompt_detail : state.bridge_prompt_title;
      append_activity_event(state, state.bridge_status_line);
      return;
    }

    if (kind == "bridge_response") {
      state.bridge_prompt_pending = false;
      state.bridge_prompt_kind = BridgePromptKind::None;
      state.bridge_prompt_title = "";
      state.bridge_prompt_detail = "";
      state.bridge_prompt_option_count = 0;
      state.bridge_prompt_selected_index = 0;
      state.bridge_status_line = content.length() > 0 ? content : "Bridge response recorded";
      state.status_line = state.bridge_status_line;
      append_activity_event(state, state.bridge_status_line);
      return;
    }

    if (kind == "voice_prompt_transcribed") {
      state.status_line = content;
      state.codex_stream_line = content;
      state.codex_state = CodexState::Busy;
      append_activity_event(state, content);
      return;
    }

    if (kind == "bridge_connected") {
      state.bridge_status_line = content.length() > 0 ? content : "Bridge connected";
      state.codex_state = CodexState::Idle;
      append_activity_event(state, state.bridge_status_line);
      return;
    }

    state.status_line = content.length() > 0 ? content : state.status_line;
    if (kind == "completed") {
      state.codex_state = CodexState::Idle;
    } else if (kind == "status") {
      state.codex_state = CodexState::Busy;
    }
    if (content.length() > 0) {
      state.codex_stream_line = content;
      append_activity_event(state, content);
    }
    return;
  }

  if (event_type == "codex_delta") {
    state.codex_stream_line = content;
    state.codex_state = CodexState::Busy;
    if (content.length() > 0) {
      state.status_line = content;
      append_activity_event(state, content);
    }
    return;
  }

  if (event_type == "codex_usage") {
    if (payload["data"].is<JsonObjectConst>()) {
      JsonObjectConst data = payload["data"].as<JsonObjectConst>();
      if (data["usedPercent"].is<int>()) {
        state.codex_usage_percent = data["usedPercent"].as<int>();
      }
      if (data["windowMinutes"].is<int>()) {
        state.codex_usage_window_minutes = data["windowMinutes"].as<int>();
      }
      if (data["resetsAt"].is<uint32_t>()) {
        state.codex_usage_resets_at = data["resetsAt"].as<uint32_t>();
      }
    }
    state.codex_usage_detail_line = content;
    if (content.length() > 0) {
      append_activity_event(state, content);
    }
    return;
  }

  if (event_type == "approval_request") {
    const String approval_id = payload["approval_id"] | payload["approvalId"] | "";
    state.approval_pending = approval_id.length() > 0;
    state.approval_id = approval_id;
    state.approval_title = payload["title"] | "Approval requested";
    state.approval_detail_line = payload["detail"] | payload["message"] | "";
    state.approval_timeout_seconds = payload["timeout_seconds"] | payload["timeoutSeconds"] | 0;
    state.codex_state = CodexState::WaitingForApproval;
    state.status_line = "Approval pending";
    append_activity_event(state, state.approval_title);
    return;
  }

  if (event_type == "bridge_notification") {
    state.bridge_prompt_kind = BridgePromptKind::Notification;
    state.bridge_prompt_pending = false;
    state.bridge_prompt_title = payload["title"] | "Notification";
    state.bridge_prompt_detail = payload["detail"] | "";
    state.bridge_prompt_option_count = 0;
    state.bridge_prompt_selected_index = 0;
    state.bridge_status_line = state.bridge_prompt_title;
    state.status_line = state.bridge_prompt_detail.length() > 0 ? state.bridge_prompt_detail : state.bridge_prompt_title;
    append_activity_event(state, state.status_line);
    return;
  }

  if (event_type == "bridge_question") {
    state.bridge_prompt_kind = BridgePromptKind::Question;
    state.bridge_prompt_pending = true;
    state.bridge_prompt_title = payload["title"] | "Question";
    state.bridge_prompt_detail = payload["detail"] | "";
    state.bridge_prompt_selected_index = 0;
    state.bridge_status_line = "Bridge question pending";
    state.bridge_prompt_option_count = 0;
    JsonArrayConst options = payload["options"];
    if (!options.isNull()) {
      size_t i = 0;
      for (JsonVariantConst option : options) {
        if (i >= state.bridge_prompt_options.size()) {
          break;
        }
        state.bridge_prompt_options[i++] = option.is<const char*>() ? option.as<const char*>() : "";
      }
      state.bridge_prompt_option_count = i;
    }
    state.status_line = state.bridge_prompt_title;
    append_activity_event(state, state.bridge_prompt_title);
    return;
  }

  if (event_type == "bridge_confirmation") {
    state.bridge_prompt_kind = BridgePromptKind::Confirmation;
    state.bridge_prompt_pending = true;
    state.bridge_prompt_title = payload["title"] | "Confirmation";
    state.bridge_prompt_detail = payload["detail"] | "";
    state.bridge_prompt_selected_index = 0;
    state.bridge_prompt_options[0] = "Accept";
    state.bridge_prompt_options[1] = "Reject";
    state.bridge_prompt_options[2] = "";
    state.bridge_prompt_option_count = 2;
    state.bridge_status_line = "Bridge confirmation pending";
    state.status_line = state.bridge_prompt_detail.length() > 0 ? state.bridge_prompt_detail : state.bridge_prompt_title;
    append_activity_event(state, state.bridge_status_line);
    return;
  }

  if (event_type == "bridge_response") {
    state.bridge_prompt_pending = false;
    state.bridge_prompt_kind = BridgePromptKind::None;
    state.bridge_prompt_title = "";
    state.bridge_prompt_detail = "";
    state.bridge_prompt_option_count = 0;
    state.bridge_prompt_selected_index = 0;
    state.bridge_status_line = content.length() > 0 ? content : "Bridge response recorded";
    state.status_line = state.bridge_status_line;
    append_activity_event(state, state.bridge_status_line);
    return;
  }

  if (event_type == "error") {
    state.status_line = content.length() > 0 ? content : "Middleware error";
    state.bridge_status_line = state.status_line;
    append_activity_event(state, state.status_line);
    return;
  }
}

String MiddlewareLink::buildMessage(const String& id, const String& type, const String& payload_json) const {
  return buildEnvelope(id, type, payload_json);
}

String MiddlewareLink::buildEnvelope(const String& id, const String& type, const String& payload_json) const {
  String output = String("{\"protocol_version\":1,\"id\":\"") + escapeJson(id) +
                  String("\",\"type\":\"") + escapeJson(type) + String("\",\"payload\":") + payload_json;
  if (auth_token_.length() > 0) {
    output += String(",\"auth_token\":\"") + escapeJson(auth_token_) + "\"";
  }
  output += "}";
  return output;
}

String MiddlewareLink::nextMessageId() {
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "msg-%06lu", static_cast<unsigned long>(next_message_id_++));
  return String(buffer);
}

String MiddlewareLink::escapeJson(const String& value) const {
  String output;
  output.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    switch (c) {
      case '\\':
      case '"':
        output += '\\';
        output += c;
        break;
      case '\b':
        output += "\\b";
        break;
      case '\f':
        output += "\\f";
        break;
      case '\n':
        output += "\\n";
        break;
      case '\r':
        output += "\\r";
        break;
      case '\t':
        output += "\\t";
        break;
      default:
        output += c;
        break;
    }
  }
  return output;
}
