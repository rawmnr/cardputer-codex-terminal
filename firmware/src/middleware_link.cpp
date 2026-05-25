#include "middleware_link.h"
#include "protocol.h"
#include "runtime/firmware_runtime.h"

#include <memory>
#include <mbedtls/base64.h>
#include <ESPmDNS.h>

#if !defined(NATIVE_BUILD)
#include <NimBLEDevice.h>
#endif
#include "device_config.h"
#include <string>
namespace {
constexpr size_t kJsonCapacity = 2048;
constexpr size_t kInboundJsonSlack = 512;
constexpr size_t kBleChunkSize = 20;
constexpr size_t kBleMaxLineLength = 1024;
constexpr char kBleServiceUuid[] = "a5cd0001-c0de-4abe-9c1a-4d5e6f7a8b90";
constexpr char kBleRxUuid[] = "a5cd0002-c0de-4abe-9c1a-4d5e6f7a8b90";
constexpr char kBleTxUuid[] = "a5cd0003-c0de-4abe-9c1a-4d5e6f7a8b90";
}  // namespace
#if !defined(NATIVE_BUILD)
class MiddlewareBleServerCallbacks final : public NimBLEServerCallbacks {
 public:
  void onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo) override {
    (void)server;
    (void)connInfo;
    if (MiddlewareLink::instance_ != nullptr) {
      MiddlewareLink::instance_->onBleConnected();
    }
  }

  void onDisconnect(NimBLEServer* server, NimBLEConnInfo& connInfo, int reason) override {
    (void)server;
    (void)connInfo;
    (void)reason;
    if (MiddlewareLink::instance_ != nullptr) {
      MiddlewareLink::instance_->onBleDisconnected();
    }
    NimBLEDevice::getAdvertising()->start();
  }
};

class MiddlewareBleRxCallbacks final : public NimBLECharacteristicCallbacks {
 public:
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo) override {
    (void)connInfo;
    if (MiddlewareLink::instance_ == nullptr) {
      return;
    }
    const std::string value = characteristic->getValue();
    MiddlewareLink::instance_->onBleIncomingChunk(
      reinterpret_cast<const uint8_t*>(value.data()),
      value.length()
    );
  }
};
#endif

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
  ensureBleTransport(state);

  if (!configured_) {
    if (!ble_started_) {
      state.bridge_status_line = "Middleware bridge disabled";
    }
    drainBleInbound(state);
    if (ble_status_request_pending_ && ble_connected_) {
      if (sendStatusRequest()) {
        ble_status_request_pending_ = false;
      }
    }
    return;
  }

  if (!ensureConnection(state)) {
    client_.loop();
  } else {
    client_.loop();
  }

  drainBleInbound(state);
  if (ble_status_request_pending_ && ble_connected_) {
    if (sendStatusRequest()) {
      ble_status_request_pending_ = false;
    }
  }
}

bool MiddlewareLink::sendTextPrompt(const String& text) {
  DynamicJsonDocument payload(kJsonCapacity);
  payload["text"] = text;
  return sendCardputerMessage(buildEnvelope(nextMessageId(), "text_prompt", payload.as<JsonVariantConst>()));
}

bool MiddlewareLink::sendVoicePromptIntent(const String& intent, const String& target) {
  DynamicJsonDocument payload(kJsonCapacity);
  payload["intent"] = intent;
  payload["target"] = target;
  return sendCardputerMessage(buildEnvelope(nextMessageId(), "voice_prompt_intent", payload.as<JsonVariantConst>()));
}

bool MiddlewareLink::sendProjectSelect(const String& workspace_path) {
  DynamicJsonDocument payload(kJsonCapacity);
  payload["workspace_path"] = workspace_path;
  return sendCardputerMessage(buildEnvelope(nextMessageId(), "project_select", payload.as<JsonVariantConst>()));
}

bool MiddlewareLink::sendBranchSelect(const String& branch) {
  DynamicJsonDocument payload(kJsonCapacity);
  payload["branch"] = branch;
  return sendCardputerMessage(buildEnvelope(nextMessageId(), "branch_select", payload.as<JsonVariantConst>()));
}

bool MiddlewareLink::sendThreadSelect(const String& thread_id) {
  DynamicJsonDocument payload(kJsonCapacity);
  payload["thread_id"] = thread_id;
  return sendCardputerMessage(buildEnvelope(nextMessageId(), "thread_select", payload.as<JsonVariantConst>()));
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
  
  DynamicJsonDocument payload(kJsonCapacity);
  payload["chunk_id"] = chunk_id;
  payload["pcm_b64"] = pcm_b64;
  payload["sample_rate_hz"] = sample_rate_hz;
  return sendCardputerMessage(buildEnvelope(nextMessageId(), "audio_chunk", payload.as<JsonVariantConst>()));
}

bool MiddlewareLink::sendVoicePromptReady(uint32_t sample_rate_hz, size_t sample_count, int peak_amplitude) {
  DynamicJsonDocument payload(kJsonCapacity);
  payload["sample_rate_hz"] = sample_rate_hz;
  payload["sample_count"] = sample_count;
  payload["peak_amplitude"] = peak_amplitude;
  return sendCardputerMessage(buildEnvelope(nextMessageId(), "voice_prompt_ready", payload.as<JsonVariantConst>()));
}

bool MiddlewareLink::sendApprovalResponse(bool approved, const String& approval_id) {
  DynamicJsonDocument payload(kJsonCapacity);
  payload["approved"] = approved;
  if (approval_id.length() > 0) {
    payload["approval_id"] = approval_id;
  }
  return sendCardputerMessage(buildEnvelope(nextMessageId(), "approval_response", payload.as<JsonVariantConst>()));
}

bool MiddlewareLink::sendRunAction(const String& action, const String& run_id, const String& approval_id) {
  String command = "/run ";
  command += action;
  command += ' ';
  command += run_id;
  if (approval_id.length() > 0) {
    command += ' ';
    command += approval_id;
  }
  return sendTextPrompt(command);
}

bool MiddlewareLink::sendRunListRequest() {
  DynamicJsonDocument payload(kJsonCapacity);
  return sendCardputerMessage(buildEnvelope(nextMessageId(), "run_list_request", payload.as<JsonVariantConst>()));
}

bool MiddlewareLink::sendRunDetailRequest(const String& run_id) {
  DynamicJsonDocument payload(kJsonCapacity);
  payload["run_id"] = run_id;
  return sendCardputerMessage(buildEnvelope(nextMessageId(), "run_detail_request", payload.as<JsonVariantConst>()));
}

bool MiddlewareLink::sendApprovalInboxRequest() {
  DynamicJsonDocument payload(kJsonCapacity);
  return sendCardputerMessage(buildEnvelope(nextMessageId(), "approval_inbox_request", payload.as<JsonVariantConst>()));
}

bool MiddlewareLink::sendBridgeNotification(const String& title, const String& detail) {
  DynamicJsonDocument payload(kJsonCapacity);
  payload["title"] = title;
  payload["detail"] = detail;
  return sendCardputerMessage(buildEnvelope(nextMessageId(), "bridge_notification", payload.as<JsonVariantConst>()));
}

bool MiddlewareLink::sendBridgeQuestion(const String& title, const String& detail, const std::array<String, 3>& options, size_t option_count) {
  DynamicJsonDocument payload(kJsonCapacity);
  payload["title"] = title;
  payload["detail"] = detail;
  JsonArray options_arr = payload["options"].to<JsonArray>();
  for (size_t i = 0; i < option_count && i < options.size(); ++i) {
    options_arr.add(options[i]);
  }
  return sendCardputerMessage(buildEnvelope(nextMessageId(), "bridge_question", payload.as<JsonVariantConst>()));
}

bool MiddlewareLink::sendBridgeConfirmation(const String& title, const String& detail) {
  DynamicJsonDocument payload(kJsonCapacity);
  payload["title"] = title;
  payload["detail"] = detail;
  return sendCardputerMessage(buildEnvelope(nextMessageId(), "bridge_confirmation", payload.as<JsonVariantConst>()));
}

bool MiddlewareLink::sendBridgeResponse(bool accepted, size_t selected_index, const String& note) {
  DynamicJsonDocument payload(kJsonCapacity);
  payload["accepted"] = accepted;
  payload["selected_index"] = selected_index;
  payload["note"] = note;
  return sendCardputerMessage(buildEnvelope(nextMessageId(), "bridge_response", payload.as<JsonVariantConst>()));
}

bool MiddlewareLink::sendDisplaySnapshot(
  const String& screen_text,
  const String& status_line,
  const String& active_app,
  const String& input_line,
  const String& firmware_name,
  const String& network_status_line
) {
  DynamicJsonDocument payload(kJsonCapacity);
  payload["screen_text"] = screen_text;
  payload["status_line"] = status_line;
  payload["active_app"] = active_app;
  payload["input_line"] = input_line;
  payload["firmware_name"] = firmware_name;
  payload["network_status_line"] = network_status_line;
  return sendCardputerMessage(buildEnvelope(nextMessageId(), "display_snapshot", payload.as<JsonVariantConst>()));
}

bool MiddlewareLink::sendInterrupt(const String& thread_id) {
  DynamicJsonDocument payload(kJsonCapacity);
  payload["thread_id"] = thread_id;
  return sendCardputerMessage(buildEnvelope(nextMessageId(), "interrupt", payload.as<JsonVariantConst>()));
}

bool MiddlewareLink::sendStatusRequest() {
  DynamicJsonDocument payload(kJsonCapacity);
  return sendCardputerMessage(buildEnvelope(nextMessageId(), "status_request", payload.as<JsonVariantConst>()));
}

void MiddlewareLink::handleWebSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  if (instance_ != nullptr) {
    instance_->onWebSocketEvent(type, payload, length);
  }
}

void MiddlewareLink::onWebSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      started_ = true;
      {
        StaticJsonDocument<256> hello_payload;
        hello_payload["device"] = "m5stack-cardputer";
        hello_payload["firmware"] = "0.2.0";
        JsonArray caps = hello_payload.createNestedArray("capabilities");
        caps.add("display_240x135");
        caps.add("keyboard");
        caps.add("mic_pcm16");
        caps.add("approval");
        caps.add("bridge_response");
        caps.add("ble_control");

        String msg = buildEnvelope(nextMessageId(), "hello", hello_payload.as<JsonVariant>());
        client_.sendTXT(msg);
      }
      break;
    case WStype_DISCONNECTED:
      started_ = false;
      break;
    case WStype_TEXT:
      if (state_ != nullptr) {
        handleIncomingJson(*state_, payload, length);
      }
      break;
    default:
      break;
  }
}
void MiddlewareLink::handleIncomingJson(DeviceState& state, const uint8_t* payload, size_t length) {
  size_t capacity = length + kInboundJsonSlack;
  if (capacity < kJsonCapacity) {
    capacity = kJsonCapacity;
  }

  DynamicJsonDocument doc(capacity);
  const DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    state.bridge_status_line = String("Bridge JSON error: ") + error.c_str();
    append_activity_event(state, state.bridge_status_line);
    return;
  }

  const String event_type = doc["type"] | "";
  const String message_id = doc["id"] | "";
  JsonObjectConst payload_variant = doc["payload"];
  if (payload_variant.isNull()) {
    state.bridge_status_line = "Bridge payload malformed";
    append_activity_event(state, state.bridge_status_line);
    return;
  }

  if (event_type == "ack") {
    const bool ok = payload_variant["ok"] | false;
    state.bridge_status_line = String("Ack ") + message_id + (ok ? " ok" : " failed");
    append_activity_event(state, state.bridge_status_line);
    return;
  }

  applyIncomingEvent(state, event_type, payload_variant);
}

bool MiddlewareLink::sendCardputerMessage(const String& message) {
  const String transport = state_ != nullptr && state_->active_transport.length() > 0 ? state_->active_transport : String("wifi");
  const bool ble_allowed = state_ != nullptr &&
                           state_->ble_enabled &&
                           (transport == "ble" || transport == "hybrid") &&
                           isBleControlMessage(message);
  bool sent = false;

  if (transport == "ble") {
    if (ble_allowed && ble_connected_) {
      sent = sendBleCardputerMessage(message);
      if (sent) {
        return true;
      }
    }

    if (client_.isConnected()) {
      String mutable_message = message;
      return client_.sendTXT(mutable_message);
    }

    return false;
  }

  if (transport == "hybrid" && ble_allowed && ble_connected_) {
    sent = sendBleCardputerMessage(message) || sent;
  }

  if (client_.isConnected()) {
    String mutable_message = message;
    sent = client_.sendTXT(mutable_message) || sent;
  }

  if (!sent && ble_allowed && ble_connected_) {
    sent = sendBleCardputerMessage(message);
  }

  return sent;
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
#if !defined(NATIVE_BUILD)
String MiddlewareLink::buildBleAdvertisedName(const DeviceState& state) const {
  const String base_name = state.ble_name.length() > 0 ? state.ble_name : String("CardputerCodex");
  char suffix[7];
  snprintf(suffix, sizeof(suffix), "%06llX", static_cast<unsigned long long>(ESP.getEfuseMac() & 0xFFFFFFULL));
  return base_name + "_" + suffix;
}

bool MiddlewareLink::isBleControlMessage(const String& message) const {
  if (message.length() == 0 || message.length() > kBleMaxLineLength) {
    return false;
  }

  DynamicJsonDocument doc(512);
  if (deserializeJson(doc, message) != DeserializationError::Ok) {
    return false;
  }

  const String type = doc["type"] | "";
  return type == "status_request" ||
         type == "interrupt" ||
         type == "bridge_notification" ||
         type == "bridge_question" ||
         type == "bridge_confirmation" ||
         type == "bridge_response" ||
         type == "approval_response" ||
         type == "ping";
}

void MiddlewareLink::ensureBleTransport(DeviceState& state) {
  const bool transport_uses_ble = state.active_transport == "ble" || state.active_transport == "hybrid";
  if (!state.ble_enabled || !transport_uses_ble || ble_started_) {
    return;
  }

  NimBLEUUID service_uuid(kBleServiceUuid);
  NimBLEUUID rx_uuid(kBleRxUuid);
  NimBLEUUID tx_uuid(kBleTxUuid);
  ble_device_name_ = buildBleAdvertisedName(state);
  NimBLEDevice::init(ble_device_name_.c_str());
  ble_server_ = NimBLEDevice::createServer();
  ble_server_->setCallbacks(new MiddlewareBleServerCallbacks());
  NimBLEService* service = ble_server_->createService(service_uuid);
  ble_rx_characteristic_ = service->createCharacteristic(
    rx_uuid,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
  );
  ble_rx_characteristic_->setCallbacks(new MiddlewareBleRxCallbacks());
  ble_tx_characteristic_ = service->createCharacteristic(tx_uuid, NIMBLE_PROPERTY::NOTIFY);
  service->start();
  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(service_uuid);
  advertising->setName(ble_device_name_.c_str());
  advertising->start();

  ble_started_ = true;
  ble_connected_ = false;
  ble_status_request_pending_ = false;
  ble_rx_buffer_ = "";
  ble_inbound_head_ = 0;
  ble_inbound_count_ = 0;
  state.ble_advertising = true;
  state.ble_connected = false;
  state.ble_status_line = String("BLE advertising as ") + ble_device_name_;
  state.bridge_status_line = state.ble_status_line;
  append_activity_event(state, state.ble_status_line);
}

void MiddlewareLink::drainBleInbound(DeviceState& state) {
  String line;
  while (dequeueBleLine(line)) {
    handleIncomingJson(state, reinterpret_cast<const uint8_t*>(line.c_str()), line.length());
  }
}

void MiddlewareLink::onBleIncomingChunk(const uint8_t* data, size_t length) {
  if (state_ == nullptr || data == nullptr || length == 0) {
    return;
  }

  std::string chunk(reinterpret_cast<const char*>(data), length);
  ble_rx_buffer_ += chunk.c_str();
  while (true) {
    const int newline = ble_rx_buffer_.indexOf('\n');
    if (newline < 0) {
      break;
    }

    String line = ble_rx_buffer_.substring(0, newline);
    ble_rx_buffer_.remove(0, newline + 1);
    line.trim();
    if (line.length() == 0) {
      continue;
    }

    enqueueBleLine(line);
  }

  if (ble_rx_buffer_.length() > kBleMaxLineLength) {
    ble_rx_buffer_ = "";
    state_->ble_status_line = "BLE input overflow";
    state_->bridge_status_line = state_->ble_status_line;
    append_activity_event(*state_, state_->ble_status_line);
  }
}

void MiddlewareLink::onBleConnected() {
  if (runtime_ != nullptr) {
    const String message = String("BLE connected: ") + ble_device_name_;
    runtime_->enqueueEvent(AppEvent::bleStatus(millis(), true, false, message.c_str()));
  }
}

void MiddlewareLink::onBleDisconnected() {
  if (runtime_ != nullptr) {
    runtime_->enqueueEvent(AppEvent::bleStatus(millis(), false, true, "BLE disconnected"));
  }
}


void MiddlewareLink::enqueueBleLine(const String& line) {
  if (ble_inbound_count_ >= ble_inbound_lines_.size()) {
    ble_inbound_lines_[ble_inbound_head_] = line;
    ble_inbound_head_ = (ble_inbound_head_ + 1) % ble_inbound_lines_.size();
    return;
  }

  const size_t index = (ble_inbound_head_ + ble_inbound_count_) % ble_inbound_lines_.size();
  ble_inbound_lines_[index] = line;
  ++ble_inbound_count_;
}

bool MiddlewareLink::dequeueBleLine(String& line) {
  if (ble_inbound_count_ == 0) {
    return false;
  }

  line = ble_inbound_lines_[ble_inbound_head_];
  ble_inbound_lines_[ble_inbound_head_] = "";
  ble_inbound_head_ = (ble_inbound_head_ + 1) % ble_inbound_lines_.size();
  --ble_inbound_count_;
  return true;
}

bool MiddlewareLink::sendBleCardputerMessage(const String& message) {
  if (!ble_started_ || !ble_connected_ || ble_tx_characteristic_ == nullptr) {
    return false;
  }

  String framed = message;
  framed += '\n';
  const char* raw = framed.c_str();
  const size_t total_length = framed.length();
  for (size_t offset = 0; offset < total_length; offset += kBleChunkSize) {
    const size_t chunk_length = min(kBleChunkSize, total_length - offset);
    ble_tx_characteristic_->setValue(reinterpret_cast<const uint8_t*>(raw + offset), chunk_length);
    ble_tx_characteristic_->notify();
  }

  return true;
}
#else

String MiddlewareLink::buildBleAdvertisedName(const DeviceState& state) const {
  (void)state;
  return "";
}

bool MiddlewareLink::isBleControlMessage(const String& message) const {
  (void)message;
  return false;
}

void MiddlewareLink::ensureBleTransport(DeviceState& state) {
  (void)state;
}

void MiddlewareLink::drainBleInbound(DeviceState& state) {
  (void)state;
}

void MiddlewareLink::onBleIncomingChunk(const uint8_t* data, size_t length) {
  (void)data;
  (void)length;
}

void MiddlewareLink::onBleConnected() {}

void MiddlewareLink::onBleDisconnected() {}

void MiddlewareLink::enqueueBleLine(const String& line) {
  (void)line;
}

bool MiddlewareLink::dequeueBleLine(String& line) {
  (void)line;
  return false;
}

bool MiddlewareLink::sendBleCardputerMessage(const String& message) {
  (void)message;
  return false;
}
#endif
void MiddlewareLink::setRuntime(FirmwareRuntime* runtime) {
  runtime_ = runtime;
}

void MiddlewareLink::applyIncomingEvent(DeviceState& state, const String& event_type, JsonObjectConst payload) {
  if (event_type == "hello_ack") {
    state.bridge_status_line = "Bridge " + payload["version"].as<String>();
    // Handshake complete
    sendStatusRequest();
    return;
  }

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
      if (payload["codex_usage_percent"].is<int>()) {
        state.codex_usage_percent = payload["codex_usage_percent"].as<int>();
      }
      if (payload["codex_usage_secondary_percent"].is<int>()) {
        state.codex_usage_secondary_percent = payload["codex_usage_secondary_percent"].as<int>();
      }
      if (payload["codex_usage_window_minutes"].is<int>()) {
        state.codex_usage_window_minutes = payload["codex_usage_window_minutes"].as<int>();
      }
      if (payload["codex_usage_secondary_window_minutes"].is<int>()) {
        state.codex_usage_secondary_window_minutes = payload["codex_usage_secondary_window_minutes"].as<int>();
      }
      if (payload["codex_usage_resets_at"].is<uint32_t>()) {
        state.codex_usage_resets_at = payload["codex_usage_resets_at"].as<uint32_t>();
      }
      if (payload["codex_usage_secondary_resets_at"].is<uint32_t>()) {
        state.codex_usage_secondary_resets_at = payload["codex_usage_secondary_resets_at"].as<uint32_t>();
      }
      if (payload["codex_usage_reset_line"].is<const char*>()) {
        state.codex_usage_reset_line = payload["codex_usage_reset_line"].as<const char*>();
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

  if (event_type == "run_list") {
    state.runs.active_run_id = payload["active_run_id"] | state.runs.active_run_id;
    state.runs.run_count = 0;
    JsonArrayConst runs = payload["runs"];
    if (!runs.isNull()) {
      size_t i = 0;
      for (JsonVariantConst run_variant : runs) {
        if (i >= state.runs.runs.size()) {
          break;
        }
        if (!run_variant.is<JsonObjectConst>()) {
          continue;
        }
        JsonObjectConst run = run_variant.as<JsonObjectConst>();
        RunSummaryView& view = state.runs.runs[i];
        view.run_id = run["id"] | run["run_id"] | "";
        view.title = run["title"] | "";
        view.branch = run["branch"] | "";
        view.mode = run["mode"] | "";
        view.status = run["status"] | "";
        view.last_event = run["last"] | "";
        view.thread_id = run["thread_id"] | "";
        view.approval_id = run["approval_id"] | "";
        view.approval_title = run["approval_title"] | "";
        view.danger_level = run["danger_level"] | "normal";
        view.merge_ready = run["merge_ready"] | false;
        view.worktree_exists = String(run["wt"] | "ok") != "missing";
        ++i;
      }
      state.runs.run_count = i;
    }
    state.status_line = content.length() > 0 ? content : "Runs updated";
    append_activity_event(state, state.status_line);
    return;
  }

  if (event_type == "run_detail") {
    state.runs.detail.run_id = payload["id"] | "";
    state.runs.detail.role = payload["role"] | "";
    state.runs.detail.mode = payload["mode"] | "";
    state.runs.detail.status = payload["status"] | "";
    state.runs.detail.branch = payload["branch"] | "";
    state.runs.detail.step = payload["step"] | "";
    state.runs.detail.last_event = payload["last"] | "";
    state.runs.detail.thread_id = payload["thread_id"] | "";
    state.runs.detail.session_id = payload["session_id"] | "";
    state.runs.detail.workspace_path = payload["workspace_path"] | "";
    state.runs.detail.danger = payload["danger"] | "";
    state.runs.detail.badge = payload["badge"] | "";
    state.runs.detail.stale = payload["stale"] | "";
    state.runs.detail.merge_ready = payload["merge_ready"] | false;
    state.runs.detail.worktree_exists = payload["wt"] | true;
    state.runs.detail.approval_pending = false;
    JsonObjectConst approval = payload["approval"];
    if (!approval.isNull()) {
      state.runs.detail.approval_id = approval["id"] | "";
      state.runs.detail.approval_title = approval["title"] | "";
      state.runs.detail.approval_detail = approval["detail"] | "";
      state.runs.detail.approval_danger = approval["danger_level"] | "";
      state.runs.detail.approval_pending = state.runs.detail.approval_id.length() > 0;
    } else {
      state.runs.detail.approval_id = "";
      state.runs.detail.approval_title = "";
      state.runs.detail.approval_detail = "";
      state.runs.detail.approval_danger = "";
    }
    if (payload["diff"].is<JsonObjectConst>()) {
      JsonObjectConst diff = payload["diff"];
      state.runs.detail.diff_files = diff["files"] | 0;
      state.runs.detail.diff_insertions = diff["ins"] | 0;
      state.runs.detail.diff_deletions = diff["del"] | 0;
      state.runs.detail.diff_summary = diff["sum"] | "";
    } else {
      state.runs.detail.diff_files = 0;
      state.runs.detail.diff_insertions = 0;
      state.runs.detail.diff_deletions = 0;
      state.runs.detail.diff_summary = "";
    }
    if (payload["test"].is<JsonObjectConst>()) {
      JsonObjectConst test = payload["test"];
      state.runs.detail.test_run = test["run"] | 0;
      state.runs.detail.test_passed = test["pass"] | 0;
      state.runs.detail.test_failed = test["fail"] | 0;
      state.runs.detail.test_skipped = test["skip"] | 0;
      state.runs.detail.test_summary = test["sum"] | "";
    } else {
      state.runs.detail.test_run = 0;
      state.runs.detail.test_passed = 0;
      state.runs.detail.test_failed = 0;
      state.runs.detail.test_skipped = 0;
      state.runs.detail.test_summary = "";
    }
    state.runs.selected_run_id = state.runs.detail.run_id;
    state.runs.screen = RunsScreen::Detail;
    state.status_line = content.length() > 0 ? content : "Run detail updated";
    append_activity_event(state, state.status_line);
    return;
  }

  if (event_type == "approval_inbox") {
    state.approvals.approval_count = 0;
    JsonArrayConst approvals = payload["approvals"];
    if (!approvals.isNull()) {
      size_t i = 0;
      for (JsonVariantConst approval_variant : approvals) {
        if (i >= state.approvals.approvals.size()) {
          break;
        }
        if (!approval_variant.is<JsonObjectConst>()) {
          continue;
        }
        JsonObjectConst approval = approval_variant.as<JsonObjectConst>();
        ApprovalInboxItem& view = state.approvals.approvals[i];
        view.run_id = approval["run_id"] | "";
        view.run_title = approval["run_title"] | approval["title"] | "";
        view.approval_id = approval["id"] | approval["approval_id"] | "";
        view.title = approval["title"] | "";
        view.detail = approval["detail"] | "";
        view.danger_level = approval["danger_level"] | ((approval["danger"] | false) ? "high" : "normal");
        view.mode = approval["mode"] | "";
        view.status = approval["status"] | "";
        view.branch = approval["branch"] | "";
        view.thread_id = approval["thread_id"] | "";
        ++i;
      }
      state.approvals.approval_count = i;
    }
    state.status_line = content.length() > 0 ? content : "Approvals updated";
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
      state.ui_mode = UiMode::Modal;
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
      state.ui_mode = UiMode::BridgePrompt;
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
      state.ui_mode = UiMode::BridgePrompt;
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
      state.ui_mode = UiMode::Home;
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
    if (payload["codex_usage_percent"].is<int>()) {
      state.codex_usage_percent = payload["codex_usage_percent"].as<int>();
    }
    if (payload["codex_usage_secondary_percent"].is<int>()) {
      state.codex_usage_secondary_percent = payload["codex_usage_secondary_percent"].as<int>();
    }
    if (payload["codex_usage_window_minutes"].is<int>()) {
      state.codex_usage_window_minutes = payload["codex_usage_window_minutes"].as<int>();
    }
    if (payload["codex_usage_secondary_window_minutes"].is<int>()) {
      state.codex_usage_secondary_window_minutes = payload["codex_usage_secondary_window_minutes"].as<int>();
    }
    if (payload["codex_usage_resets_at"].is<uint32_t>()) {
      state.codex_usage_resets_at = payload["codex_usage_resets_at"].as<uint32_t>();
    }
    if (payload["codex_usage_secondary_resets_at"].is<uint32_t>()) {
      state.codex_usage_secondary_resets_at = payload["codex_usage_secondary_resets_at"].as<uint32_t>();
    }
    if (payload["codex_usage_reset_line"].is<const char*>()) {
      state.codex_usage_reset_line = payload["codex_usage_reset_line"].as<const char*>();
    }

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
    state.ui_mode = UiMode::Approval;
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
    state.ui_mode = UiMode::Modal;
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
    state.ui_mode = UiMode::BridgePrompt;
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
    state.ui_mode = UiMode::BridgePrompt;
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
    state.ui_mode = UiMode::Home;
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

String MiddlewareLink::buildEnvelope(const String& id, const String& type, const JsonVariantConst& payload) const {
  return buildCardputerEnvelope(id, type, payload, auth_token_);
}

String MiddlewareLink::nextMessageId() {
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "msg-%06lu", static_cast<unsigned long>(next_message_id_++));
  return String(buffer);
}
