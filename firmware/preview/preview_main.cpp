#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <cctype>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <lvgl.h>
#include <src/display/lv_display.h>
#include <src/indev/lv_indev.h>

#include "app_shell.h"
#include "apps.h"
#include "device_state.h"
#include "lvgl_screen.h"

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
  if (name == "space") return ' ';
  if (name == "ctrl+m") return LV_KEY_ENTER;
  if (name.size() == 1) return static_cast<uint32_t>(name[0]);
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

std::string sanitizeLabel(std::string value) {
  for (char& c : value) {
    const unsigned char uc = static_cast<unsigned char>(c);
    if (!(std::isalnum(uc) || c == '_' || c == '-' || c == '.')) {
      c = '_';
    }
  }
  if (value.empty()) {
    value = "frame";
  }
  return value;
}

const char* readStringField(const JsonObjectConst& step, const char* first, const char* second = nullptr, const char* third = nullptr) {
  const char* value = step[first] | "";
  if (value[0] != '\0') {
    return value;
  }
  if (second != nullptr) {
    value = step[second] | "";
    if (value[0] != '\0') {
      return value;
    }
  }
  if (third != nullptr) {
    value = step[third] | "";
    if (value[0] != '\0') {
      return value;
    }
  }
  return "";
}

void exportPng(const char* path, const uint16_t* framebuffer, int width, int height) {
  if (!framebuffer) {
    std::cerr << "Error: null framebuffer" << std::endl;
    return;
  }

  std::vector<uint8_t> rgb888(static_cast<size_t>(width) * static_cast<size_t>(height) * 3);
  for (int i = 0; i < width * height; ++i) {
    const uint16_t rgb565 = framebuffer[i];
    const uint8_t r = static_cast<uint8_t>((rgb565 >> 11) & 0x1F);
    const uint8_t g = static_cast<uint8_t>((rgb565 >> 5) & 0x3F);
    const uint8_t b = static_cast<uint8_t>(rgb565 & 0x1F);
    rgb888[static_cast<size_t>(i) * 3 + 0] = static_cast<uint8_t>((r * 255) / 31);
    rgb888[static_cast<size_t>(i) * 3 + 1] = static_cast<uint8_t>((g * 255) / 63);
    rgb888[static_cast<size_t>(i) * 3 + 2] = static_cast<uint8_t>((b * 255) / 31);
  }

  stbi_write_png(path, width, height, 3, rgb888.data(), width * 3);
}

void advanceShell(AppShell& shell, int ticks = 20) {
  shell.render();
  for (int i = 0; i < ticks; ++i) {
    shell.tick();
  }
}

void tapKey(AppShell& shell, uint32_t key) {
  shell.handleUiKey(static_cast<lv_key_t>(key), true);
  advanceShell(shell, 10);
  shell.handleUiKey(static_cast<lv_key_t>(key), false);
  advanceShell(shell, 10);
}

String stepLabel(const JsonObjectConst& step, int index) {
  const char* label = readStringField(step, "label", "name", "capture");
  if (label[0] != '\0') {
    return String(index) + "_" + sanitizeLabel(label).c_str();
  }
  const char* type = readStringField(step, "type");
  return String(index) + "_" + sanitizeLabel(type).c_str();
}

int stepTicks(const JsonObjectConst& step) {
  if (step.containsKey("ticks")) {
    return std::max(1, step["ticks"].as<int>());
  }
  if (step.containsKey("ms")) {
    return std::max(1, step["ms"].as<int>() / 16);
  }
  return 20;
}

void emitFrame(AppShell& shell, const std::string& output_prefix, const String& label) {
  shell.render();
  for (int i = 0; i < 20; ++i) {
    shell.tick();
  }

  const std::string path = output_prefix + "_" + label.c_str() + ".png";
  std::cout << "Exporting " << path << "..." << std::endl;
  exportPng(path.c_str(), shell.framebuffer(), 240, 135);
}

void applyStep(AppShell& shell, const JsonObjectConst& step) {
  const String type = readStringField(step, "type");
  if (type == "wait") {
    advanceShell(shell, stepTicks(step));
    return;
  }

  if (type == "command") {
    const String value = readStringField(step, "value", "command");
    shell.handleCommand(value);
    advanceShell(shell, stepTicks(step));
    return;
  }

  if (type == "text") {
    const String value = readStringField(step, "value", "text");
    const bool submit = step["submit"] | false;
    const bool backspace = step["backspace"] | false;
    shell.handleTextInput(value, submit, backspace);
    advanceShell(shell, stepTicks(step));
    return;
  }

  if (type == "action") {
    const String value = readStringField(step, "value");
    if (value == "up") shell.handleAction(UiAction::Up);
    else if (value == "down") shell.handleAction(UiAction::Down);
    else if (value == "left") shell.handleAction(UiAction::Left);
    else if (value == "right") shell.handleAction(UiAction::Right);
    else if (value == "select" || value == "enter") shell.handleAction(UiAction::Select);
    else if (value == "back" || value == "esc") shell.handleAction(UiAction::Back);
    else if (value == "menu") shell.handleAction(UiAction::Menu);
    else if (value == "submit") shell.handleAction(UiAction::Submit);
    advanceShell(shell, stepTicks(step));
    return;
  }

  if (type == "key" || type == "press") {
    const std::string key_name = readStringField(step, "value", "key");
    const uint32_t key = keyFromName(key_name);
    if (key != 0) {
      tapKey(shell, key);
    }
    advanceShell(shell, stepTicks(step));
    return;
  }

  advanceShell(shell, stepTicks(step));
}

void applyLegacyActions(AppShell& shell, JsonArrayConst actions) {
  for (JsonVariantConst v : actions) {
    if (!v.is<const char*>()) {
      continue;
    }

    const std::string action = v.as<const char*>();
    std::cout << "Action: " << action << std::endl;

    if (action == "menu") {
      shell.handleAction(UiAction::Menu);
    } else if (action == "tab") {
      if (shell.isAppMenuOpen()) {
        shell.handleAction(UiAction::Down);
      } else {
        const uint32_t key = keyFromName(action);
        if (key != 0) {
          tapKey(shell, key);
        }
      }
    } else if (action == "enter") {
      if (shell.isAppMenuOpen()) {
        shell.handleAction(UiAction::Select);
      } else {
        const uint32_t key = keyFromName(action);
        if (key != 0) {
          tapKey(shell, key);
        }
      }
    } else if (action == "esc") {
      if (shell.isAppMenuOpen()) {
        shell.handleAction(UiAction::Back);
      } else {
        const uint32_t key = keyFromName(action);
        if (key != 0) {
          tapKey(shell, key);
        }
      }
    } else {
      const uint32_t key = keyFromName(action);
      if (key != 0) {
        tapKey(shell, key);
      }
    }

    advanceShell(shell, 20);
  }
}
}  // namespace

int main(int argc, char** argv) {
  std::string fixture_path = "firmware/preview/fixtures/buddy_idle.json";
  std::string output_prefix = "preview";

  if (argc > 1) fixture_path = argv[1];
  if (argc > 2) output_prefix = argv[2];

  std::cout << "Loading fixture: " << fixture_path << std::endl;

  std::ifstream fixture_file(fixture_path);
  const std::string content((std::istreambuf_iterator<char>(fixture_file)), std::istreambuf_iterator<char>());
  DynamicJsonDocument doc(content.size() * 2 + 4096);
  const DeserializationError error = deserializeJson(doc, content);
  if (error) {
    std::cerr << "Failed to parse fixture JSON: " << error.c_str() << std::endl;
    return 1;
  }

  AppShell shell;
  shell.begin();

  auto render = [&](const std::string& suffix) {
    advanceShell(shell, 20);
    const std::string path = output_prefix + (suffix.empty() ? std::string() : "_" + suffix) + ".png";
    std::cout << "Exporting " << path << "..." << std::endl;
    exportPng(path.c_str(), shell.framebuffer(), 240, 135);
  };

  render("0_initial");

  if (doc.containsKey("active_app")) {
    JsonVariantConst active_app = doc["active_app"];
    if (active_app.is<int>()) {
      const String command = appNameFromId(active_app.as<int>());
      if (command.length() > 0) {
        shell.handleCommand(command);
        render("1_active_app");
      }
    } else if (active_app.is<const char*>()) {
      const String value = active_app.as<const char*>();
      if (value.length() > 0) {
        shell.handleCommand(String("/app ") + value);
        render("1_active_app");
      }
    }
  }

  if (doc.containsKey("steps") && doc["steps"].is<JsonArrayConst>()) {
    JsonArrayConst steps = doc["steps"].as<JsonArrayConst>();
    int step_index = 1;
    for (JsonObjectConst step : steps) {
      applyStep(shell, step);
      emitFrame(shell, output_prefix, stepLabel(step, step_index++));
    }
  } else if (doc.containsKey("actions") && doc["actions"].is<JsonArrayConst>()) {
    applyLegacyActions(shell, doc["actions"].as<JsonArrayConst>());
    render("1_actions");
  }

  return 0;
}
