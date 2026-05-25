#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <SDL3/SDL.h>

#include <Arduino.h>
#include <ArduinoJson.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include "app_shell.h"
#include "apps.h"

namespace {
constexpr int kCardputerWidth = 240;
constexpr int kCardputerHeight = 135;

struct SimulatorArgs {
  std::string fixture_path;
  std::string output_prefix;
  int scale = 4;
  bool interactive = false;
  bool headless = false;
};

uint32_t keyFromName(const std::string& name) {
  if (name == "up") return SDLK_UP;
  if (name == "down") return SDLK_DOWN;
  if (name == "left") return SDLK_LEFT;
  if (name == "right") return SDLK_RIGHT;
  if (name == "enter" || name == "return") return SDLK_RETURN;
  if (name == "esc" || name == "escape") return SDLK_ESCAPE;
  if (name == "backspace" || name == "back") return SDLK_BACKSPACE;
  if (name == "tab") return SDLK_TAB;
  if (name == "space") return SDLK_SPACE;
  if (name == "del" || name == "delete") return SDLK_DELETE;
  if (name == "menu") return SDLK_F1;
  if (name == "ctrl+m") return SDLK_F1;
  if (name == "fn+;" || name == "prev") return SDLK_F2;
  if (name == "fn+." || name == "next") return SDLK_F3;
  if (name == "hold-space") return SDLK_F4;
  if (name.size() == 1) {
    return static_cast<uint32_t>(name[0]);
  }
  return 0;
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

int stepTicks(const JsonObjectConst& step) {
  if (step.containsKey("ticks")) {
    return std::max(1, step["ticks"].as<int>());
  }
  if (step.containsKey("ms")) {
    return std::max(1, step["ms"].as<int>() / 16);
  }
  return 20;
}

String stepLabel(const JsonObjectConst& step, int index) {
  const char* label = readStringField(step, "label", "name", "capture");
  if (label[0] != '\0') {
    return String(index) + "_" + sanitizeLabel(label).c_str();
  }
  const char* type = readStringField(step, "type");
  return String(index) + "_" + sanitizeLabel(type).c_str();
}

void exportPng(const std::string& path, const uint16_t* framebuffer) {
  if (framebuffer == nullptr) {
    std::cerr << "error: framebuffer missing" << std::endl;
    return;
  }

  std::vector<uint8_t> rgb888(static_cast<size_t>(kCardputerWidth) * static_cast<size_t>(kCardputerHeight) * 3);
  for (int i = 0; i < kCardputerWidth * kCardputerHeight; ++i) {
    const uint16_t rgb565 = framebuffer[i];
    const uint8_t r = static_cast<uint8_t>((rgb565 >> 11) & 0x1F);
    const uint8_t g = static_cast<uint8_t>((rgb565 >> 5) & 0x3F);
    const uint8_t b = static_cast<uint8_t>(rgb565 & 0x1F);
    rgb888[static_cast<size_t>(i) * 3 + 0] = static_cast<uint8_t>((r * 255) / 31);
    rgb888[static_cast<size_t>(i) * 3 + 1] = static_cast<uint8_t>((g * 255) / 63);
    rgb888[static_cast<size_t>(i) * 3 + 2] = static_cast<uint8_t>((b * 255) / 31);
  }

  stbi_write_png(path.c_str(), kCardputerWidth, kCardputerHeight, 3, rgb888.data(), kCardputerWidth * 3);
}

void advanceShell(AppShell& shell, SDL_Window* window, SDL_Renderer* renderer, SDL_Texture* texture, int scale, int ticks) {
  for (int i = 0; i < ticks; ++i) {
    shell.tick();
    shell.render();
    const uint16_t* framebuffer = shell.framebuffer();
    SDL_UpdateTexture(texture, nullptr, framebuffer, kCardputerWidth * static_cast<int>(sizeof(uint16_t)));

    int window_w = 0;
    int window_h = 0;
    SDL_GetWindowSizeInPixels(window, &window_w, &window_h);
    const int draw_scale = std::max(1, std::min({scale, window_w / kCardputerWidth, window_h / kCardputerHeight}));
    SDL_FRect dst {
      static_cast<float>((window_w - (kCardputerWidth * draw_scale)) / 2),
      static_cast<float>((window_h - (kCardputerHeight * draw_scale)) / 2),
      static_cast<float>(kCardputerWidth * draw_scale),
      static_cast<float>(kCardputerHeight * draw_scale),
    };

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, nullptr, &dst);
    SDL_RenderPresent(renderer);
    SDL_Delay(16);
  }
}

void tapKey(AppShell& shell, SDL_Window* window, SDL_Renderer* renderer, SDL_Texture* texture, int scale, uint32_t key) {
  shell.handleUiKey(static_cast<lv_key_t>(key), true);
  advanceShell(shell, window, renderer, texture, scale, 2);
  shell.handleUiKey(static_cast<lv_key_t>(key), false);
  advanceShell(shell, window, renderer, texture, scale, 2);
}

void handleScenarioEvent(AppShell& shell, const JsonObjectConst& step) {
  const String value = readStringField(step, "value", "name", "event");
  if (value == "wifi_connecting") {
    shell.handleCommand("/status Wi-Fi connecting");
    return;
  }
  if (value == "wifi_connected") {
    shell.handleCommand("/wifi on");
    return;
  }
  if (value == "wifi_offline") {
    shell.handleCommand("/wifi off");
    return;
  }
  if (value == "codex_request_started") {
    shell.handleCommand("/codex busy");
    return;
  }
  if (value == "codex_response_completed") {
    shell.handleCommand("/status Codex response completed");
    return;
  }
  if (value == "error") {
    const String detail = readStringField(step, "text", "detail");
    shell.handleCommand(String("/notify ") + detail);
    return;
  }
  if (value == "token_chunk") {
    const String chunk = readStringField(step, "text", "detail");
    shell.handleCommand(String("/status ") + chunk);
    return;
  }
  shell.handleCommand(String("/status ") + value);
}

void applyStep(AppShell& shell, SDL_Window* window, SDL_Renderer* renderer, SDL_Texture* texture, int scale, const JsonObjectConst& step) {
  const String type = readStringField(step, "type");
  if (type == "wait") {
    advanceShell(shell, window, renderer, texture, scale, stepTicks(step));
    return;
  }

  if (type == "command") {
    shell.handleCommand(readStringField(step, "value", "command"));
    advanceShell(shell, window, renderer, texture, scale, stepTicks(step));
    return;
  }

  if (type == "text") {
    const String value = readStringField(step, "value", "text");
    const bool submit = step["submit"] | false;
    const bool backspace = step["backspace"] | false;
    shell.handleTextInput(value, submit, backspace);
    advanceShell(shell, window, renderer, texture, scale, stepTicks(step));
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
    advanceShell(shell, window, renderer, texture, scale, stepTicks(step));
    return;
  }

  if (type == "key" || type == "press") {
    const std::string key_name = readStringField(step, "value", "key");
    const uint32_t key = keyFromName(key_name);
    if (key != 0) {
      tapKey(shell, window, renderer, texture, scale, key);
    } else {
      advanceShell(shell, window, renderer, texture, scale, stepTicks(step));
    }
    return;
  }

  if (type == "event" || type == "mock" || type == "network") {
    handleScenarioEvent(shell, step);
    advanceShell(shell, window, renderer, texture, scale, stepTicks(step));
    return;
  }

  advanceShell(shell, window, renderer, texture, scale, stepTicks(step));
}

void applyLegacyActions(AppShell& shell, SDL_Window* window, SDL_Renderer* renderer, SDL_Texture* texture, int scale, JsonArrayConst actions) {
  for (JsonVariantConst v : actions) {
    if (!v.is<const char*>()) {
      continue;
    }

    const std::string action = v.as<const char*>();
    std::cout << "action: " << action << std::endl;

    if (action == "menu") {
      shell.handleAction(UiAction::Menu);
    } else if (action == "tab") {
      if (shell.isAppMenuOpen()) {
        shell.handleAction(UiAction::Down);
      } else {
        const uint32_t key = keyFromName(action);
        if (key != 0) {
          tapKey(shell, window, renderer, texture, scale, key);
        }
      }
    } else if (action == "enter") {
      if (shell.isAppMenuOpen()) {
        shell.handleAction(UiAction::Select);
      } else {
        const uint32_t key = keyFromName(action);
        if (key != 0) {
          tapKey(shell, window, renderer, texture, scale, key);
        }
      }
    } else if (action == "esc") {
      if (shell.isAppMenuOpen()) {
        shell.handleAction(UiAction::Back);
      } else {
        const uint32_t key = keyFromName(action);
        if (key != 0) {
          tapKey(shell, window, renderer, texture, scale, key);
        }
      }
    } else {
      const uint32_t key = keyFromName(action);
      if (key != 0) {
        tapKey(shell, window, renderer, texture, scale, key);
      }
    }

    advanceShell(shell, window, renderer, texture, scale, 8);
  }
}

bool renderFrame(AppShell& shell, SDL_Window* window, SDL_Renderer* renderer, SDL_Texture* texture, int scale) {
  const uint16_t* framebuffer = shell.framebuffer();
  if (framebuffer == nullptr) {
    std::cerr << "error: frame buffer unavailable" << std::endl;
    return false;
  }

  SDL_UpdateTexture(texture, nullptr, framebuffer, kCardputerWidth * static_cast<int>(sizeof(uint16_t)));

  int window_w = 0;
  int window_h = 0;
  SDL_GetWindowSizeInPixels(window, &window_w, &window_h);
  const int draw_scale = std::max(1, std::min({scale, window_w / kCardputerWidth, window_h / kCardputerHeight}));
  const SDL_FRect dst {
    static_cast<float>((window_w - (kCardputerWidth * draw_scale)) / 2),
    static_cast<float>((window_h - (kCardputerHeight * draw_scale)) / 2),
    static_cast<float>(kCardputerWidth * draw_scale),
    static_cast<float>(kCardputerHeight * draw_scale),
  };

  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);
  SDL_RenderTexture(renderer, texture, nullptr, &dst);
  SDL_RenderPresent(renderer);
  return true;
}

void printHelp() {
  std::cout << "Usage: simulator [fixture.json] [output_prefix] [--scale N] [--interactive] [--headless]\n";
}

SimulatorArgs parseArgs(int argc, char** argv) {
  SimulatorArgs args;
  int positional = 0;
  for (int i = 1; i < argc; ++i) {
    const std::string current = argv[i];
    if (current == "--help" || current == "-h") {
      printHelp();
      std::exit(0);
    } else if (current == "--scale" && i + 1 < argc) {
      args.scale = std::max(1, std::stoi(argv[++i]));
    } else if (current == "--interactive") {
      args.interactive = true;
    } else if (current == "--headless" || current == "--no-window") {
      args.headless = true;
    } else if (current == "--fixture" && i + 1 < argc) {
      args.fixture_path = argv[++i];
      positional = std::max(positional, 1);
    } else if (current == "--output-prefix" && i + 1 < argc) {
      args.output_prefix = argv[++i];
      positional = std::max(positional, 2);
    } else if (!current.empty() && current[0] != '-') {
      if (positional == 0) {
        args.fixture_path = current;
      } else if (positional == 1) {
        args.output_prefix = current;
      }
      ++positional;
    }
  }

  if (args.fixture_path.empty()) {
    args.interactive = true;
  }
  if (args.output_prefix.empty()) {
    args.output_prefix = "preview";
  }
  return args;
}

std::string readFile(const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error("failed to open fixture: " + path);
  }
  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

void dispatchKey(AppShell& shell, SDL_Window* window, SDL_Renderer* renderer, SDL_Texture* texture, int scale, const SDL_KeyboardEvent& event) {
  const SDL_Keycode key = event.key;
  uint32_t mapped = 0;
  if (key == SDLK_UP) mapped = LV_KEY_UP;
  else if (key == SDLK_DOWN) mapped = LV_KEY_DOWN;
  else if (key == SDLK_LEFT) mapped = LV_KEY_LEFT;
  else if (key == SDLK_RIGHT) mapped = LV_KEY_RIGHT;
  else if (key == SDLK_RETURN || key == SDLK_RETURN2) mapped = LV_KEY_ENTER;
  else if (key == SDLK_ESCAPE) mapped = LV_KEY_ESC;
  else if (key == SDLK_BACKSPACE) mapped = LV_KEY_BACKSPACE;
  else if (key == SDLK_TAB) mapped = LV_KEY_NEXT;
  else if (key == SDLK_DELETE) mapped = LV_KEY_DEL;
  else if (key == SDLK_SPACE) mapped = ' ';
  else if (key == SDLK_F1) mapped = LV_KEY_ENTER;
  else if (key == SDLK_F2) mapped = LV_KEY_PREV;
  else if (key == SDLK_F3) mapped = LV_KEY_NEXT;
  else if (key == SDLK_F4) mapped = LV_KEY_HOME;

  if (mapped != 0) {
    shell.handleUiKey(static_cast<lv_key_t>(mapped), event.down);
    renderFrame(shell, window, renderer, texture, scale);
    std::cout << "key " << (event.down ? "down" : "up") << ": " << SDL_GetKeyName(key) << std::endl;
  }
}

int runScenario(AppShell& shell, SDL_Window* window, SDL_Renderer* renderer, SDL_Texture* texture, const SimulatorArgs& args, const JsonDocument& doc) {
  renderFrame(shell, window, renderer, texture, args.scale);
  if (doc.containsKey("active_app")) {
    JsonVariantConst active_app = doc["active_app"];
    if (active_app.is<int>()) {
      const int value = active_app.as<int>();
      const String command = value == 0 ? "/app buddy"
        : value == 1 ? "/app push"
        : value == 2 ? "/app pager"
        : value == 3 ? "/app usage"
        : value == 4 ? "/app mcp"
        : value == 5 ? "/app settings"
        : "";
      if (command.length() > 0) {
        shell.handleCommand(command);
      }
    } else if (active_app.is<const char*>()) {
      const String value = active_app.as<const char*>();
      if (value.length() > 0) {
        shell.handleCommand(String("/app ") + value);
      }
    }
    advanceShell(shell, window, renderer, texture, args.scale, 4);
    renderFrame(shell, window, renderer, texture, args.scale);
  }

  if (doc.containsKey("steps") && doc["steps"].is<JsonArrayConst>()) {
    JsonArrayConst steps = doc["steps"].as<JsonArrayConst>();
    int step_index = 1;
    for (JsonObjectConst step : steps) {
      const String label = stepLabel(step, step_index++);
      std::cout << "step " << label.c_str() << std::endl;
      applyStep(shell, window, renderer, texture, args.scale, step);
      renderFrame(shell, window, renderer, texture, args.scale);
      if (!args.output_prefix.empty()) {
        exportPng(args.output_prefix + "_" + label.c_str() + ".png", shell.framebuffer());
      }
    }
  } else if (doc.containsKey("actions") && doc["actions"].is<JsonArrayConst>()) {
    applyLegacyActions(shell, window, renderer, texture, args.scale, doc["actions"].as<JsonArrayConst>());
    renderFrame(shell, window, renderer, texture, args.scale);
    if (!args.output_prefix.empty()) {
      exportPng(args.output_prefix + "_1_actions.png", shell.framebuffer());
    }
  }

  return 0;
}
}  // namespace

int main(int argc, char** argv) {
  try {
    const SimulatorArgs args = parseArgs(argc, argv);

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
      std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
      return 1;
    }

    const int window_width = kCardputerWidth * std::max(1, args.scale);
    const int window_height = kCardputerHeight * std::max(1, args.scale);
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE;
    if (args.headless) {
      flags = static_cast<SDL_WindowFlags>(flags | SDL_WINDOW_HIDDEN);
    }

    SDL_Window* window = SDL_CreateWindow("Cardputer Codex Simulator", window_width, window_height, flags);
    if (window == nullptr) {
      std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
      SDL_Quit();
      return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr) {
      std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
      SDL_DestroyWindow(window);
      SDL_Quit();
      return 1;
    }

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, kCardputerWidth, kCardputerHeight);
    if (texture == nullptr) {
      std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << std::endl;
      SDL_DestroyRenderer(renderer);
      SDL_DestroyWindow(window);
      SDL_Quit();
      return 1;
    }

    SDL_StartTextInput(window);

    AppShell shell;
    shell.begin();
    shell.render();
    renderFrame(shell, window, renderer, texture, args.scale);

    if (!args.fixture_path.empty()) {
      std::cout << "fixture: " << args.fixture_path << std::endl;
      const std::string raw = readFile(args.fixture_path);
      JsonDocument doc;
      const DeserializationError error = deserializeJson(doc, raw);
      if (error) {
        std::cerr << "Failed to parse fixture JSON: " << error.c_str() << std::endl;
        SDL_StopTextInput(window);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
      }

      const int result = runScenario(shell, window, renderer, texture, args, doc);
      SDL_StopTextInput(window);
      SDL_DestroyTexture(texture);
      SDL_DestroyRenderer(renderer);
      SDL_DestroyWindow(window);
      SDL_Quit();
      return result;
    }

    bool running = true;
    while (running) {
      SDL_Event event;
      while (SDL_PollEvent(&event)) {
        switch (event.type) {
          case SDL_EVENT_QUIT:
            running = false;
            break;
          case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            running = false;
            break;
          case SDL_EVENT_KEY_DOWN:
          case SDL_EVENT_KEY_UP:
            dispatchKey(shell, window, renderer, texture, args.scale, event.key);
            break;
          case SDL_EVENT_TEXT_INPUT:
            if (event.text.text != nullptr && event.text.text[0] != '\0') {
              shell.handleTextInput(String(event.text.text), false, false);
              shell.render();
              renderFrame(shell, window, renderer, texture, args.scale);
              std::cout << "text: " << event.text.text << std::endl;
            }
            break;
          default:
            break;
        }
      }

      shell.tick();
      shell.render();
      renderFrame(shell, window, renderer, texture, args.scale);
      SDL_Delay(16);
    }

    SDL_StopTextInput(window);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
  } catch (const std::exception& exc) {
    std::cerr << exc.what() << std::endl;
    return 1;
  }
}
