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
  String bridge_transport = "wifi";
  bool ble_enabled = false;
  String ble_name = "CardputerCodex";
  bool ble_control_only = true;
  bool sd_config_loaded = false;
  bool sd_mounted = false;
};

class NetworkManager {
 public:
  void begin();
  void tick(DeviceState& state);
  const RuntimeNetworkConfig& config() const;
  void logMessage(const String& message);

 private:
  static constexpr const char* kConfigDirectory = "/cardputer-codex";
  static constexpr const char* kConfigPath = "/cardputer-codex/config.ini";
  static constexpr const char* kLogPath = "/cardputer-codex/log.txt";

  void loadConfigFromSdCard();
  void ensureConfigDirectory();
  void seedConfigTemplateIfMissing();
  void connect(DeviceState& state);
  static String trimCopy(String value);
  void logMessageLocked(const String& message);

  unsigned long last_attempt_ms_ = 0;
  unsigned long connect_started_ms_ = 0;
  bool connect_in_progress_ = false;
  wl_status_t last_wifi_status_ = WL_NO_SHIELD;
  String last_logged_network_status_;
  RuntimeNetworkConfig config_;
};
