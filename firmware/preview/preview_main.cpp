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
#include "app_shell.h"
namespace {
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

String appNameFromId(int active_app) {
  switch (active_app) {
    case 0: return "/app buddy";
    case 1: return "/app push";
    case 2: return "/app pager";
    case 3: return "/app usage";
    case 4: return "/app mcp";
    case 5: return "/app settings";
    default: return "";
  }
}

void tapKey(AppShell& shell, uint32_t key) {
  shell.handleUiKey(static_cast<lv_key_t>(key), true);
  for (int i = 0; i < 10; ++i) {
    shell.render();
    shell.tick();
  }
  shell.handleUiKey(static_cast<lv_key_t>(key), false);
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

  std::ifstream f(fixture_path);
  std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  StaticJsonDocument<4096> doc;
  deserializeJson(doc, content);

  AppShell shell;
  shell.begin();

  auto render = [&](const std::string& suffix) {
    shell.render();
    for (int i = 0; i < 20; ++i) shell.tick();
    std::string path = output_prefix + (suffix.empty() ? "" : "_" + suffix) + ".png";
    std::cout << "Exporting " << path << "..." << std::endl;
    exportPng(path.c_str(), shell.framebuffer(), 240, 135);
  };

  render("0_initial");

  if (doc.containsKey("active_app")) {
    JsonVariant active_app = doc["active_app"];
    if (active_app.is<int>()) {
      String command = appNameFromId(active_app.as<int>());
      if (command.length() > 0) {
        shell.handleCommand(command);
        render("1_active_app");
      }
    } else if (active_app.is<const char*>()) {
      String value = active_app.as<const char*>();
      if (value.length() > 0) {
        shell.handleCommand(String("/app ") + value);
        render("1_active_app");
      }
    }
  }

  if (doc.containsKey("actions")) {
    JsonArray actions = doc["actions"].as<JsonArray>();
    int step = 1;
    for (JsonVariant v : actions) {
      if (!v.is<const char*>()) {
        continue;
      }

      std::string action = v.as<const char*>();
      std::cout << "Action: " << action << std::endl;

      if (action == "menu") {
        shell.handleAction(UiAction::Menu);
      } else if (action == "tab") {
        if (shell.isAppMenuOpen()) {
          shell.handleAction(UiAction::Down);
        } else {
          uint32_t key = keyFromName(action);
          if (key != 0) {
            tapKey(shell, key);
          }
        }
      } else if (action == "enter") {
        if (shell.isAppMenuOpen()) {
          shell.handleAction(UiAction::Select);
        } else {
          uint32_t key = keyFromName(action);
          if (key != 0) {
            tapKey(shell, key);
          }
        }
      } else if (action == "esc") {
        if (shell.isAppMenuOpen()) {
          shell.handleAction(UiAction::Back);
        } else {
          uint32_t key = keyFromName(action);
          if (key != 0) {
            tapKey(shell, key);
          }
        }
      } else {
        uint32_t key = keyFromName(action);
        if (key != 0) {
          tapKey(shell, key);
        }
      }

      for (int i = 0; i < 20; ++i) {
        shell.render();
        shell.tick();
      }
      render(std::to_string(step++) + "_" + action);
    }
  }

  return 0;
}
