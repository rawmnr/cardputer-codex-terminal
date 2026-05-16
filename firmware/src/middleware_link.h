#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>

#include "device_state.h"

class MiddlewareLink {
 public:
  void begin(DeviceState& state);
  void tick(DeviceState& state);
  void configure(const String& host, uint16_t port, const String& path);
  bool isConfigured() const;
  bool isConnected();

  bool sendTextPrompt(const String& text);
  bool sendAudioChunk(size_t chunk_id, const int16_t* samples, size_t sample_count, uint32_t sample_rate_hz);
  bool sendVoicePromptReady(uint32_t sample_rate_hz, size_t sample_count, int peak_amplitude);
  bool sendApprovalResponse(bool approved);
  bool sendStatusRequest();

 private:
  static void handleWebSocketEvent(WStype_t type, uint8_t* payload, size_t length);
  void onWebSocketEvent(WStype_t type, uint8_t* payload, size_t length);
  bool sendCardputerMessage(const String& message);
  bool ensureConnection(DeviceState& state);
  void applyIncomingEvent(DeviceState& state, const String& event_type, JsonObjectConst payload);
  String buildMessage(const String& type, const String& payload_json) const;
  String escapeJson(const String& value) const;

  String host_;
  String path_;
  uint16_t port_ = 0;
  bool configured_ = false;
  bool started_ = false;
  DeviceState* state_ = nullptr;
  WebSocketsClient client_;
  static MiddlewareLink* instance_;
};
