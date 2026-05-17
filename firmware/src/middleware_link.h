#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>

#include "device_state.h"

class MiddlewareLink {
 public:
  void begin(DeviceState& state);
  void tick(DeviceState& state);
  void configure(const String& host, uint16_t port, const String& path, const String& auth_token);
  bool isConfigured() const;
  bool isConnected();

  bool sendTextPrompt(const String& text);
  bool sendAudioChunk(size_t chunk_id, const int16_t* samples, size_t sample_count, uint32_t sample_rate_hz);
  bool sendVoicePromptReady(uint32_t sample_rate_hz, size_t sample_count, int peak_amplitude);
  bool sendApprovalResponse(bool approved);
  bool sendBridgeNotification(const String& title, const String& detail);
  bool sendBridgeQuestion(const String& title, const String& detail, const std::array<String, 3>& options, size_t option_count);
  bool sendBridgeConfirmation(const String& title, const String& detail);
  bool sendBridgeResponse(bool accepted, size_t selected_index, const String& note);
  bool sendDisplaySnapshot(
    const String& screen_text,
    const String& status_line,
    const String& active_app,
    const String& input_line,
    const String& firmware_name,
    const String& network_status_line
  );
  bool sendStatusRequest();

 private:
  static void handleWebSocketEvent(WStype_t type, uint8_t* payload, size_t length);
  void onWebSocketEvent(WStype_t type, uint8_t* payload, size_t length);
  bool sendCardputerMessage(const String& message);
  String nextMessageId();
  bool ensureConnection(DeviceState& state);
  void applyIncomingEvent(DeviceState& state, const String& event_type, JsonObjectConst payload);
  String buildMessage(const String& id, const String& type, const String& payload_json) const;
  String buildEnvelope(const String& id, const String& type, const String& payload_json) const;
  String escapeJson(const String& value) const;

  String host_;
  String path_;
  String auth_token_;
  uint16_t port_ = 0;
  bool configured_ = false;
  bool started_ = false;
  uint32_t next_message_id_ = 1;
  DeviceState* state_ = nullptr;
  WebSocketsClient client_;
  static MiddlewareLink* instance_;
};
