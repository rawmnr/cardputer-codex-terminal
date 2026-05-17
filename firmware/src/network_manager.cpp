#include "network_manager.h"

#include <SPI.h>
#include <SD.h>

#include "device_config.h"

namespace {
constexpr unsigned long kRetryIntervalMs = 15000;
constexpr int kSdSpiSckPin = 40;
constexpr int kSdSpiMisoPin = 39;
constexpr int kSdSpiMosiPin = 14;
constexpr int kSdSpiCsPin = 12;
constexpr char kConfigPath[] = "/cardputer-codex/config.ini";
}

String NetworkManager::trimCopy(String value) {
  value.trim();
  return value;
}

void NetworkManager::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);
  loadConfigFromSdCard();
}

const RuntimeNetworkConfig& NetworkManager::config() const {
  return config_;
}

void NetworkManager::loadConfigFromSdCard() {
  config_.sd_mounted = false;
  config_.sd_config_loaded = false;

  SPI.begin(kSdSpiSckPin, kSdSpiMisoPin, kSdSpiMosiPin, kSdSpiCsPin);
  if (!SD.begin(kSdSpiCsPin, SPI, 25000000)) {
    return;
  }

  config_.sd_mounted = true;
  if (!SD.exists(kConfigPath)) {
    return;
  }

  File file = SD.open(kConfigPath, FILE_READ);
  if (!file) {
    return;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line = trimCopy(line);
    if (line.length() == 0 || line.startsWith("#") || line.startsWith(";")) {
      continue;
    }

    const int separator = line.indexOf('=');
    if (separator < 0) {
      continue;
    }

    const String key = trimCopy(line.substring(0, separator));
    const String value = trimCopy(line.substring(separator + 1));

    if (key == "wifi_ssid") {
      config_.wifi_ssid = value;
    } else if (key == "wifi_password") {
      config_.wifi_password = value;
    } else if (key == "middleware_host") {
      config_.middleware_host = value;
    } else if (key == "middleware_port") {
      config_.middleware_port = static_cast<uint16_t>(value.toInt());
    } else if (key == "middleware_path") {
      config_.middleware_path = value;
    } else if (key == "middleware_token") {
      config_.middleware_token = value;
    }
  }

  file.close();
  config_.sd_config_loaded = config_.wifi_ssid.length() > 0 || config_.middleware_host.length() > 0;
}

void NetworkManager::tick(DeviceState& state) {
  const wl_status_t wifi_status = WiFi.status();
  state.wifi_connected = wifi_status == WL_CONNECTED;

  if (state.wifi_connected) {
    state.wifi_ssid = WiFi.SSID();
    state.wifi_ip = WiFi.localIP().toString();
    state.network_status_line = "Wi-Fi connected to " + state.wifi_ssid;
    connect_in_progress_ = false;
    return;
  }

  state.wifi_ssid = "";
  state.wifi_ip = "";

  const String ssid = config_.wifi_ssid.length() > 0 ? config_.wifi_ssid : String(CARDPUTER_WIFI_SSID);
  const String password = config_.wifi_password.length() > 0 ? config_.wifi_password : String(CARDPUTER_WIFI_PASSWORD);

  if (ssid.length() == 0) {
    state.network_status_line = "Wi-Fi disabled - no credentials configured";
    return;
  }

  if (connect_in_progress_) {
    state.network_status_line = "Wi-Fi connecting...";
    return;
  }

  const unsigned long now = millis();
  if (last_attempt_ms_ == 0 || now - last_attempt_ms_ >= kRetryIntervalMs) {
    connect(state);
  } else {
    state.network_status_line = "Wi-Fi retry pending";
  }
}

void NetworkManager::connect(DeviceState& state) {
  connect_in_progress_ = true;
  last_attempt_ms_ = millis();
  state.network_status_line = config_.sd_config_loaded
                               ? "Connecting Wi-Fi from SD config..."
                               : "Connecting Wi-Fi...";

  WiFi.disconnect(true, true);
  const String ssid = config_.wifi_ssid.length() > 0 ? config_.wifi_ssid : String(CARDPUTER_WIFI_SSID);
  const String password = config_.wifi_password.length() > 0 ? config_.wifi_password : String(CARDPUTER_WIFI_PASSWORD);
  WiFi.begin(ssid.c_str(), password.c_str());
}
