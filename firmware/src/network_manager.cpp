#include "network_manager.h"

#include <SPI.h>
#include <SD.h>

#include "device_config.h"

namespace {
constexpr unsigned long kRetryIntervalMs = 15000;
constexpr unsigned long kConnectTimeoutMs = 12000;
constexpr int kSdSpiSckPin = 40;
constexpr int kSdSpiMisoPin = 39;
constexpr int kSdSpiMosiPin = 14;
constexpr int kSdSpiCsPin = 12;

const char* wifiStatusName(wl_status_t status) {
  switch (status) {
    case WL_NO_SHIELD:
      return "no shield";
    case WL_IDLE_STATUS:
      return "idle";
    case WL_NO_SSID_AVAIL:
      return "ssid unavailable";
    case WL_SCAN_COMPLETED:
      return "scan completed";
    case WL_CONNECTED:
      return "connected";
    case WL_CONNECT_FAILED:
      return "connect failed";
    case WL_CONNECTION_LOST:
      return "connection lost";
    case WL_DISCONNECTED:
      return "disconnected";
    default:
      return "unknown";
  }
}
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
  ensureConfigDirectory();
  seedConfigTemplateIfMissing();

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

void NetworkManager::ensureConfigDirectory() {
  if (!SD.exists(kConfigDirectory)) {
    SD.mkdir(kConfigDirectory);
  }
}

void NetworkManager::seedConfigTemplateIfMissing() {
  if (SD.exists(kConfigPath)) {
    return;
  }

  File file = SD.open(kConfigPath, FILE_WRITE);
  if (!file) {
    return;
  }

  file.println("# Cardputer Codex terminal config");
  file.println("# Edit this file to match your Wi-Fi and middleware settings.");
  file.println("wifi_ssid=");
  file.println("wifi_password=");
  file.println("middleware_host=");
  file.println("middleware_port=8765");
  file.println("middleware_path=/");
  file.println("middleware_token=");
  file.close();
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
    if (millis() - connect_started_ms_ >= kConnectTimeoutMs) {
      connect_in_progress_ = false;
      last_attempt_ms_ = millis();
      state.network_status_line = String("Wi-Fi ") + wifiStatusName(wifi_status) + ", retrying...";
      return;
    }

    state.network_status_line = String("Wi-Fi connecting (") + wifiStatusName(wifi_status) + ")";
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
  connect_started_ms_ = millis();
  last_attempt_ms_ = millis();
  state.network_status_line = config_.sd_config_loaded
                               ? "Connecting Wi-Fi from SD config..."
                               : "Connecting Wi-Fi...";

  WiFi.disconnect(true, true);
  WiFi.setAutoReconnect(true);
  const String ssid = config_.wifi_ssid.length() > 0 ? config_.wifi_ssid : String(CARDPUTER_WIFI_SSID);
  const String password = config_.wifi_password.length() > 0 ? config_.wifi_password : String(CARDPUTER_WIFI_PASSWORD);
  WiFi.begin(ssid.c_str(), password.c_str());
}
