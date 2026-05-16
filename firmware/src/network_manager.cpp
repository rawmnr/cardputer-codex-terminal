#include "network_manager.h"

#include "device_config.h"

namespace {
constexpr unsigned long kRetryIntervalMs = 15000;
}

void NetworkManager::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);
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

  if (String(CARDPUTER_WIFI_SSID).length() == 0) {
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
  state.network_status_line = "Connecting Wi-Fi...";

  WiFi.disconnect(true, true);
  WiFi.begin(CARDPUTER_WIFI_SSID, CARDPUTER_WIFI_PASSWORD);
}
