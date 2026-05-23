#include "network_manager.h"

#include <SPI.h>
#include <SD.h>
#include <ESPmDNS.h>

#include "device_config.h"

namespace {
constexpr unsigned long kRetryIntervalMs = 15000;
constexpr unsigned long kConnectTimeoutMs = 12000;
bool parseBool(const String& value) {
  String normalized = value;
  normalized.trim();
  normalized.toLowerCase();
  return normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on";
}
String normalizeTransport(String value) {
  value.trim();
  value.toLowerCase();
  if (value == "ble" || value == "hybrid" || value == "wifi") {
    return value;
  }
  return "wifi";
}
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
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("cardputer-codex");
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  loadConfigFromSdCard();
  logMessage("Network manager initialized");
}

const RuntimeNetworkConfig& NetworkManager::config() const {
  return config_;
}

void NetworkManager::logMessage(const String& message) {
  if (!config_.sd_mounted) {
    return;
  }

  ensureConfigDirectory();

  File file = SD.open(kLogPath, FILE_APPEND);
  if (!file) {
    file = SD.open(kLogPath, FILE_WRITE);
  }

  if (!file) {
    return;
  }

  file.print('[');
  file.print(millis());
  file.print("] ");
  file.println(message);
  file.close();
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
  logMessage("SD card mounted");

  if (!SD.exists(kConfigPath)) {
    logMessage("Config file not found, template seeded");
    return;
  }

  File file = SD.open(kConfigPath, FILE_READ);
  if (!file) {
    return;
  }

  bool loaded_any_config = false;

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
      loaded_any_config = true;
    } else if (key == "wifi_password") {
      config_.wifi_password = value;
      loaded_any_config = true;
    } else if (key == "middleware_host") {
      config_.middleware_host = value;
      loaded_any_config = true;
    } else if (key == "middleware_port") {
      config_.middleware_port = static_cast<uint16_t>(value.toInt());
      loaded_any_config = true;
    } else if (key == "middleware_path") {
      config_.middleware_path = value;
      loaded_any_config = true;
    } else if (key == "middleware_token") {
      config_.middleware_token = value;
      loaded_any_config = true;
    } else if (key == "bridge_transport") {
      config_.bridge_transport = normalizeTransport(value);
      loaded_any_config = true;
    } else if (key == "ble_enabled") {
      config_.ble_enabled = parseBool(value);
      loaded_any_config = true;
    } else if (key == "ble_name") {
      config_.ble_name = value;
      loaded_any_config = true;
    } else if (key == "ble_control_only") {
      config_.ble_control_only = parseBool(value);
      loaded_any_config = true;
    }
  }

  file.close();
  config_.sd_config_loaded = loaded_any_config;
  if (config_.sd_config_loaded) {
    if (config_.wifi_ssid.length() > 0) {
      logMessage(String("Loaded SD config for SSID ") + config_.wifi_ssid);
    } else {
      logMessage("Loaded SD config");
    }
  } else {
    logMessage("Loaded empty SD config");
  }
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
  file.println("# Edit this file to match your Wi-Fi, bridge, and BLE settings.");
  file.println("wifi_ssid=");
  file.println("wifi_password=");
  file.println("middleware_host=");
  file.println("middleware_port=8765");
  file.println("middleware_path=/");
  file.println("middleware_token=");
  file.println("bridge_transport=hybrid");
  file.println("ble_enabled=true");
  file.println("ble_name=CardputerCodex");
  file.println("ble_control_only=true");
  file.close();
}

void NetworkManager::tick(DeviceState& state) {
  const wl_status_t wifi_status = WiFi.status();
  state.wifi_connected = wifi_status == WL_CONNECTED;
  state.ble_enabled = config_.ble_enabled;
  state.ble_name = config_.ble_name.length() > 0 ? config_.ble_name : String("CardputerCodex");
  state.active_transport = normalizeTransport(config_.bridge_transport);

  if (wifi_status != last_wifi_status_) {
    logMessage(String("Wi-Fi status changed: ") + wifiStatusName(wifi_status));
    last_wifi_status_ = wifi_status;
  }

  if (state.wifi_connected) {
    state.wifi_ssid = WiFi.SSID();
    state.wifi_ip = WiFi.localIP().toString();
    state.network_status_line = "Wi-Fi connected to " + state.wifi_ssid + " @ " + state.wifi_ip;
    if (last_logged_network_status_ != state.network_status_line) {
      logMessage(String("Wi-Fi connected: ") + state.wifi_ssid + " @ " + state.wifi_ip);
      last_logged_network_status_ = state.network_status_line;
    }
    connect_in_progress_ = false;
    return;
  }

  const String ssid = config_.wifi_ssid.length() > 0 ? config_.wifi_ssid : String(CARDPUTER_WIFI_SSID);
  const String password = config_.wifi_password.length() > 0 ? config_.wifi_password : String(CARDPUTER_WIFI_PASSWORD);

  state.wifi_ssid = ssid;
  state.wifi_ip = "";

  if (ssid.length() == 0) {
    state.network_status_line = "Wi-Fi disabled - no credentials configured";
    if (last_logged_network_status_ != state.network_status_line) {
      logMessage(state.network_status_line);
      last_logged_network_status_ = state.network_status_line;
    }
    return;
  }

  if (connect_in_progress_) {
    if (millis() - connect_started_ms_ >= kConnectTimeoutMs) {
      const unsigned long elapsed_ms = millis() - connect_started_ms_;
      logMessage(String("Wi-Fi connect timed out after ") + String(elapsed_ms / 1000) + "s; restarting");
      connect_in_progress_ = false;
      connect(state);
      return;
    }

    state.network_status_line = String("Wi-Fi connecting to ") + ssid + " (" + wifiStatusName(wifi_status) + ")";
    if (last_logged_network_status_ != state.network_status_line) {
      logMessage(state.network_status_line);
      last_logged_network_status_ = state.network_status_line;
    }
    return;
  }

  const unsigned long now = millis();
  if (last_attempt_ms_ == 0 || now - last_attempt_ms_ >= kRetryIntervalMs) {
    connect(state);
  } else {
    const unsigned long remaining_ms = kRetryIntervalMs - (now - last_attempt_ms_);
    state.network_status_line = String("Wi-Fi retry in ") + String((remaining_ms + 999) / 1000) + "s";
    if (last_logged_network_status_ != state.network_status_line) {
      logMessage(state.network_status_line);
      last_logged_network_status_ = state.network_status_line;
    }
  }
}

void NetworkManager::connect(DeviceState& state) {
  connect_in_progress_ = true;
  connect_started_ms_ = millis();
  last_attempt_ms_ = millis();
  const String ssid = config_.wifi_ssid.length() > 0 ? config_.wifi_ssid : String(CARDPUTER_WIFI_SSID);
  const String password = config_.wifi_password.length() > 0 ? config_.wifi_password : String(CARDPUTER_WIFI_PASSWORD);
  state.network_status_line = config_.sd_config_loaded
                               ? String("Connecting Wi-Fi to ") + ssid + " from SD config..."
                               : "Connecting Wi-Fi...";
  logMessage(String("Wi-Fi connect attempt for SSID ") + ssid + " (password " + (password.length() > 0 ? "set" : "empty") + ")");
  last_logged_network_status_ = state.network_status_line;

  WiFi.disconnect(false, false);
  WiFi.begin(ssid.c_str(), password.c_str());
}
