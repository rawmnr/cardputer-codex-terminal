#pragma once

#include <Arduino.h>
#include <array>
#include <WiFi.h>

#include "device_state.h"

struct RuntimeNetworkConfig {
  String wifi_ssid;
  String wifi_password;
  String middleware_host;
  uint16_t middleware_port = 8765;
  String middleware_path = "/";
  String middleware_token;
  bool sd_config_loaded = false;
  bool sd_mounted = false;
};

class NetworkManager {
 public:
  void begin();
  void tick(DeviceState& state);
  const RuntimeNetworkConfig& config() const;

 private:
  static constexpr const char* kConfigDirectory = "/cardputer-codex";
  static constexpr const char* kConfigPath = "/cardputer-codex/config.ini";

  void loadConfigFromSdCard();
  void ensureConfigDirectory();
  void seedConfigTemplateIfMissing();
  void connect(DeviceState& state);
  static String trimCopy(String value);

  unsigned long last_attempt_ms_ = 0;
  unsigned long connect_started_ms_ = 0;
  bool connect_in_progress_ = false;
  RuntimeNetworkConfig config_;
};
