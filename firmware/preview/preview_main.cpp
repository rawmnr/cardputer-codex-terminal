#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <Arduino.h>
#include <lvgl.h>
#include <src/display/lv_display.h>
#include <src/indev/lv_indev.h>
#include <ArduinoJson.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>

#include "device_state.h"
#include "lvgl_screen.h"
#include "apps.h"

namespace {
class MockApp : public App {
 public:
  const char* title() const override { return "Mock App"; }
  void onEnter(DeviceState&) override {}
  void onExit(DeviceState&) override {}
  void onCommand(const String&, DeviceState&) override {}
  void tick(DeviceState&) override {}
  void render(Print&, const DeviceState&) override {}
};

bool loadFixture(const std::string& path, DeviceState& state) {
  std::ifstream f(path);
  if (!f.is_open()) {
    std::cerr << "Failed to open fixture: " << path << std::endl;
    return false;
  }

  std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  StaticJsonDocument<4096> doc;
  DeserializationError error = deserializeJson(doc, content);

  if (error) {
    std::cerr << "JSON deserialization failed: " << error.c_str() << std::endl;
    return false;
  }

  if (doc.containsKey("active_app")) state.active_app = static_cast<AppId>(doc["active_app"].as<int>());
  if (doc.containsKey("ui_mode")) state.ui_mode = static_cast<UiMode>(doc["ui_mode"].as<int>());
  if (doc.containsKey("codex_state")) state.codex_state = static_cast<CodexState>(doc["codex_state"].as<int>());
  if (doc.containsKey("wifi_connected")) state.wifi_connected = doc["wifi_connected"].as<bool>();
  if (doc.containsKey("wifi_ssid")) state.wifi_ssid = doc["wifi_ssid"].as<const char*>();
  if (doc.containsKey("wifi_ip")) state.wifi_ip = doc["wifi_ip"].as<const char*>();
  if (doc.containsKey("battery_percent")) state.battery_percent = doc["battery_percent"].as<int>();
  if (doc.containsKey("codex_usage_percent")) state.codex_usage_percent = doc["codex_usage_percent"].as<int>();
  if (doc.containsKey("codex_workspace_path")) state.codex_workspace_path = doc["codex_workspace_path"].as<const char*>();
  if (doc.containsKey("codex_branch")) state.codex_branch = doc["codex_branch"].as<const char*>();
  if (doc.containsKey("codex_thread_id")) state.codex_thread_id = doc["codex_thread_id"].as<const char*>();
  if (doc.containsKey("status_line")) state.status_line = doc["status_line"].as<const char*>();
  if (doc.containsKey("bridge_status_line")) state.bridge_status_line = doc["bridge_status_line"].as<const char*>();
  if (doc.containsKey("approval_pending")) state.approval_pending = doc["approval_pending"].as<bool>();
  if (doc.containsKey("approval_title")) state.approval_title = doc["approval_title"].as<const char*>();
  if (doc.containsKey("approval_detail_line")) state.approval_detail_line = doc["approval_detail_line"].as<const char*>();

  if (doc.containsKey("menu")) {
    JsonObject menu = doc["menu"];
    if (menu.containsKey("app_menu_open")) state.menu.app_menu_open = menu["app_menu_open"].as<bool>();
    if (menu.containsKey("app_menu_selected")) state.menu.app_menu_selected = menu["app_menu_selected"].as<int>();
    if (menu.containsKey("selected_index")) state.menu.selected_index = menu["selected_index"].as<int>();
  }

  if (doc.containsKey("bridge_prompt_kind")) state.bridge_prompt_kind = static_cast<BridgePromptKind>(doc["bridge_prompt_kind"].as<int>());
  if (doc.containsKey("bridge_prompt_title")) state.bridge_prompt_title = doc["bridge_prompt_title"].as<const char*>();
  if (doc.containsKey("bridge_prompt_detail")) state.bridge_prompt_detail = doc["bridge_prompt_detail"].as<const char*>();
  if (doc.containsKey("bridge_prompt_selected_index")) state.bridge_prompt_selected_index = doc["bridge_prompt_selected_index"].as<int>();
  if (doc.containsKey("bridge_prompt_options")) {
    JsonArray options = doc["bridge_prompt_options"];
    state.bridge_prompt_option_count = 0;
    for (JsonVariant v : options) {
      if (state.bridge_prompt_option_count < state.bridge_prompt_options.size()) {
        state.bridge_prompt_options[state.bridge_prompt_option_count++] = v.as<const char*>();
      }
    }
  }

  if (doc.containsKey("pager")) {
    JsonObject pager = doc["pager"];
    if (pager.containsKey("session_count")) state.pager.session_count = pager["session_count"].as<int>();
    if (pager.containsKey("sessions")) {
      JsonArray sessions = pager["sessions"];
      for (size_t i = 0; i < sessions.size() && i < state.pager.sessions.size(); ++i) {
        JsonObject s = sessions[i];
        state.pager.sessions[i].session_id = s["session_id"].as<const char*>();
        state.pager.sessions[i].title = s["title"].as<const char*>();
        state.pager.sessions[i].status = s["status"].as<const char*>();
        state.pager.sessions[i].last_event = s["last_event"].as<const char*>();
      }
    }
  }

  return true;
}

uint32_t keyFromName(const std::string& name) {
  if (name == "up") return LV_KEY_UP;
  if (name == "down") return LV_KEY_DOWN;
  if (name == "left") return LV_KEY_LEFT;
  if (name == "right") return LV_KEY_RIGHT;
  if (name == "enter") return LV_KEY_ENTER;
  if (name == "esc") return LV_KEY_ESC;
  if (name == "backspace") return LV_KEY_BACKSPACE;
  if (name == "home") return LV_KEY_HOME;
  if (name == "end") return LV_KEY_END;
  if (name == "tab") return LV_KEY_NEXT;
  if (name == "prev") return LV_KEY_PREV;
  if (name == "del") return LV_KEY_DEL;
  if (name.length() == 1) return name[0];
  return 0;
}

void exportPng(const char* path, const uint16_t* framebuffer, int width, int height) {
  if (!framebuffer) {
    std::cerr << "Error: null framebuffer" << std::endl;
    return;
  }
  std::vector<uint8_t> rgb888(width * height * 3);
  for (int i = 0; i < width * height; ++i) {
    uint16_t rgb565 = framebuffer[i];
    uint8_t r = (rgb565 >> 11) & 0x1F;
    uint8_t g = (rgb565 >> 5) & 0x3F;
    uint8_t b = rgb565 & 0x1F;
    rgb888[i * 3 + 0] = (r * 255) / 31;
    rgb888[i * 3 + 1] = (g * 255) / 63;
    rgb888[i * 3 + 2] = (b * 255) / 31;
  }
  stbi_write_png(path, width, height, 3, rgb888.data(), width * 3);
}
}  // namespace

int main(int argc, char** argv) {
  std::string fixture_path = "firmware/preview/fixtures/buddy_idle.json";
  std::string output_prefix = "preview";

  if (argc > 1) fixture_path = argv[1];
  if (argc > 2) output_prefix = argv[2];

  std::cout << "Loading fixture: " << fixture_path << std::endl;
  DeviceState state;
  if (!loadFixture(fixture_path, state)) {
    return 1;
  }

  LvglScreen screen;
  screen.begin();

  MockApp app;
  auto render = [&](const std::string& suffix) {
    screen.renderShell(state, app, "", "");
    for (int i = 0; i < 20; ++i) screen.tick();
    std::string path = output_prefix + (suffix.empty() ? "" : "_" + suffix) + ".png";
    std::cout << "Exporting " << path << "..." << std::endl;
    exportPng(path.c_str(), screen.framebuffer(), 240, 135);
  };

  // Initial state
  render("0_initial");

  // Re-read actions from JSON
  std::ifstream f(fixture_path);
  std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  StaticJsonDocument<4096> doc;
  deserializeJson(doc, content);

  if (doc.containsKey("actions")) {
    JsonArray actions = doc["actions"].as<JsonArray>();
    int step = 1;
    for (JsonVariant v : actions) {
      if (v.is<const char*>()) {
        std::string action = v.as<const char*>();
        uint32_t key = keyFromName(action);
        if (key != 0) {
          std::cout << "Action: " << action << std::endl;
          screen.pushKey(key, true);
          for (int i = 0; i < 5; ++i) screen.tick();
          screen.pushKey(key, false);
          for (int i = 0; i < 10; ++i) screen.tick();
          render(std::to_string(step++) + "_" + action);
        }
      }
    }
  }

  return 0;
}
