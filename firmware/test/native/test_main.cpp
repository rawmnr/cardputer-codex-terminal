#include <Arduino.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <ArduinoJson.h>

#include "apps.h"
#include "network_manager.h"
#include "fakes/fake_clock.h"
#include "fakes/fake_display.h"
#include "fakes/fake_keyboard.h"
#include "fakes/fake_serial.h"
#include "input_router.h"
#include "protocol.h"
#include "terminal_snapshot.h"

namespace {
int g_failures = 0;

#define EXPECT_TRUE(expr)                                                                 \
  do {                                                                                    \
    if (!(expr)) {                                                                        \
      std::cout << __func__ << ":" << __LINE__ << " expected true: " #expr << '\n';    \
      ++g_failures;                                                                       \
    }                                                                                   \
  } while (false)

#define EXPECT_EQ(actual, expected)                                                       \
  do {                                                                                    \
    const auto& _actual = (actual);                                                       \
    const auto& _expected = (expected);                                                   \
    if (!(_actual == _expected)) {                                                        \
      std::cout << __func__ << ":" << __LINE__ << " expected equality: " #actual        \
                << " == " #expected << '\n';                                              \
      ++g_failures;                                                                       \
    }                                                                                     \
  } while (false)

std::filesystem::path fixtureRoot() {
  std::error_code ec;
  std::filesystem::path current = std::filesystem::current_path(ec);
  if (ec) {
    return std::filesystem::path("tests/contracts");
  }

  for (int depth = 0; depth < 6; ++depth) {
    const std::filesystem::path candidate = current / "tests" / "contracts";
    if (std::filesystem::exists(candidate, ec) && !ec) {
      return candidate;
    }
    if (!current.has_parent_path()) {
      break;
    }
    current = current.parent_path();
  }

  return std::filesystem::path("tests/contracts");
}

std::filesystem::path fixturePath(const std::string& relative) {
  return fixtureRoot() / relative;
}

std::string readFile(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    std::cout << "failed to open fixture: " << path.string() << '\n';
    ++g_failures;
    return {};
  }

  std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  return contents;
}

DynamicJsonDocument parseJson(const std::string& json) {
  DynamicJsonDocument doc(json.size() * 2 + 512);
  const auto error = deserializeJson(doc, json);
  if (error) {
    std::cout << "failed to parse json: " << error.c_str() << '\n';
    ++g_failures;
  }
  return doc;
}

namespace {
class StringWriter {
 public:
  size_t write(uint8_t c) {
    output_.push_back(static_cast<char>(c));
    return 1;
  }

  size_t write(const uint8_t* buffer, size_t size) {
    output_.append(reinterpret_cast<const char*>(buffer), size);
    return size;
  }

  String take() const {
    return String(output_.c_str());
  }

 private:
  std::string output_;
};
}  // namespace

String serializeVariant(JsonVariantConst variant) {
  StringWriter writer;
  serializeJson(variant, writer);
  return writer.take();
}

const char* actionName(UiAction action) {
  switch (action) {
    case UiAction::None:
      return "None";
    case UiAction::Up:
      return "Up";
    case UiAction::Down:
      return "Down";
    case UiAction::Left:
      return "Left";
    case UiAction::Right:
      return "Right";
    case UiAction::Select:
      return "Select";
    case UiAction::Back:
      return "Back";
    case UiAction::Menu:
      return "Menu";
    case UiAction::Submit:
      return "Submit";
    case UiAction::PushToTalkStart:
      return "PushToTalkStart";
    case UiAction::PushToTalkStop:
      return "PushToTalkStop";
  }
  return "Unknown";
}

CodexState parseCodexState(const String& value) {
  if (value == "Idle") {
    return CodexState::Idle;
  }
  if (value == "Busy") {
    return CodexState::Busy;
  }
  if (value == "WaitingForApproval") {
    return CodexState::WaitingForApproval;
  }
  return CodexState::Offline;
}

UiMode parseUiMode(const String& value) {
  if (value == "Menu") {
    return UiMode::Menu;
  }
  if (value == "Input") {
    return UiMode::Input;
  }
  if (value == "Modal") {
    return UiMode::Modal;
  }
  if (value == "Approval") {
    return UiMode::Approval;
  }
  if (value == "BridgePrompt") {
    return UiMode::BridgePrompt;
  }
  return UiMode::Home;
}

DeviceState buildState(JsonObjectConst state_json) {
  DeviceState state;
  state.firmware_name = state_json["firmware_name"] | "";
  state.network_status_line = state_json["network_status_line"] | "";
  state.battery_percent = state_json["battery_percent"] | 0;
  state.battery_voltage_mv = state_json["battery_voltage_mv"] | 0;
  state.codex_state = parseCodexState(state_json["codex_state"] | "Offline");
  state.wifi_connected = state_json["wifi_connected"] | false;
  state.bridge_status_line = state_json["bridge_status_line"] | "";
  state.codex_usage_percent = state_json["codex_usage_percent"] | -1;
  state.codex_workspace_path = state_json["codex_workspace_path"] | ".";
  state.codex_branch = state_json["codex_branch"] | "";
  state.codex_thread_id = state_json["codex_thread_id"] | "";
  state.approval_pending = state_json["approval_pending"] | false;
  state.approval_title = state_json["approval_title"] | "";
  state.approval_detail_line = state_json["approval_detail_line"] | "";
  state.bridge_prompt_pending = state_json["bridge_prompt_pending"] | false;
  state.ui_mode = parseUiMode(state_json["ui_mode"] | "Home");
  state.menu.app_menu_open = state_json["menu_app_menu_open"] | false;
  state.ble_enabled = state_json["ble_enabled"] | false;
  state.ble_advertising = state_json["ble_advertising"] | false;
  state.ble_connected = state_json["ble_connected"] | false;
  state.ble_name = state_json["ble_name"] | "";
  state.ble_status_line = state_json["ble_status_line"] | "";
  state.active_transport = state_json["active_transport"] | "";
  return state;
}

bool test_protocol_contracts() {
  const std::string raw = readFile(fixturePath("serial_protocol_cases.json"));
  if (raw.empty()) {
    return false;
  }

  DynamicJsonDocument doc = parseJson(raw);
  if (g_failures > 0) {
    return false;
  }

  for (JsonObjectConst entry : doc.as<JsonArrayConst>()) {
    const char* name = entry["name"] | "";
    JsonObjectConst envelope = entry["envelope"].as<JsonObjectConst>();
    const String id = envelope["id"] | "";
    const String type = envelope["type"] | "";
    const String auth_token = envelope["auth_token"] | "";

    FakeSerial serial;
    const String actual = buildCardputerEnvelope(id, type, envelope["payload"].as<JsonVariantConst>(), auth_token);
    serial.print(actual);

    DynamicJsonDocument actual_doc = parseJson(serial.str().c_str());
    if (g_failures > 0) {
      return false;
    }

    String expected = serializeVariant(envelope);
    StringWriter actual_writer;
    serializeJson(actual_doc, actual_writer);
    const String serialized_actual = actual_writer.take();

    EXPECT_EQ(serialized_actual, expected);
    std::cout << "protocol contract: " << name << '\n';
  }

  return g_failures == 0;
}

bool test_keyboard_contracts() {
  const std::string raw = readFile(fixturePath("keyboard_events.json"));
  if (raw.empty()) {
    return false;
  }

  DynamicJsonDocument doc = parseJson(raw);
  if (g_failures > 0) {
    return false;
  }

  for (JsonObjectConst entry : doc.as<JsonArrayConst>()) {
    const char* name = entry["name"] | "";
    FakeKeyboard keyboard;
    keyboard.setWord(entry["word"] | "");
    keyboard.setFn(entry["fn"] | false);

    UiAction action = UiAction::None;
    const bool matched = mapNavigationChar(keyboard.state().word.length() > 0 ? keyboard.state().word[0] : '\0', keyboard.state().fn, action);
    const String expected_action = entry["expected_action"] | "";

    EXPECT_TRUE(matched);
    EXPECT_EQ(String(actionName(action)), expected_action);
    std::cout << "keyboard contract: " << name << '\n';
  }

  return g_failures == 0;
}

bool test_menu_navigation_throttle() {
  FakeClock clock;
  clock.set(1000);

  EXPECT_TRUE(!shouldAllowMenuNavigation(clock.now(), 995));
  EXPECT_TRUE(shouldAllowMenuNavigation(clock.now(), 700));

  clock.advance(kMenuNavRepeatMs);
  EXPECT_TRUE(shouldAllowMenuNavigation(clock.now(), 1000));
  return g_failures == 0;
}

bool test_terminal_snapshots() {
  const std::filesystem::path dir = fixturePath("terminal_snapshots");
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".json") {
      continue;
    }

    const std::string raw = readFile(entry.path());
    if (raw.empty()) {
      return false;
    }

    DynamicJsonDocument doc = parseJson(raw);
    if (g_failures > 0) {
      return false;
    }

    JsonObjectConst root = doc.as<JsonObjectConst>();
    JsonObjectConst state_json = root["state"].as<JsonObjectConst>();
    DeviceState state = buildState(state_json);
    const String input_line = root["input_line"] | "";
    const String active_app_name = root["active_app"] | "";

    BuddyApp buddy;
    App* active_app = active_app_name == "Buddy" ? static_cast<App*>(&buddy) : nullptr;

    FakeDisplay display;
    writeTerminalSnapshot(display, state, active_app, input_line);

    const String expected = root["expected_snapshot"] | "";
    EXPECT_EQ(display.str(), expected);
    std::cout << "snapshot contract: " << root["name"].as<const char*>() << '\n';
  }

  return g_failures == 0;
}
bool contains(const String& haystack, const char* needle) {
  return haystack.find(needle) != String::npos;
}

bool test_protocol_edge_cases() {
  const String long_text(512, 'a');
  DynamicJsonDocument payload(1024);
  payload["text"] = String("héllo 🌍");
  payload["note"] = long_text;
  const String envelope = buildCardputerEnvelope("edge-1", "text_prompt", payload.as<JsonVariantConst>());
  EXPECT_TRUE(!contains(envelope, "\n"));

  DynamicJsonDocument parsed = parseJson(envelope.c_str());
  if (g_failures > 0) {
    return false;
  }

  EXPECT_EQ(parsed["id"].as<String>(), String("edge-1"));
  EXPECT_EQ(parsed["type"].as<String>(), String("text_prompt"));
  EXPECT_EQ(parsed["payload"]["text"].as<String>(), String("héllo 🌍"));
  EXPECT_EQ(parsed["payload"]["note"].as<String>(), long_text);
  EXPECT_TRUE(!parsed.containsKey("auth_token"));

  const String with_auth = buildCardputerEnvelope("edge-2", "text_prompt", payload.as<JsonVariantConst>(), "token");
  EXPECT_TRUE(!contains(with_auth, "\n"));
  DynamicJsonDocument parsed_auth = parseJson(with_auth.c_str());
  if (g_failures > 0) {
    return false;
  }
  EXPECT_EQ(parsed_auth["auth_token"].as<String>(), String("token"));
  return g_failures == 0;
}

bool test_ble_state_surface() {
  DynamicJsonDocument doc(256);
  doc["ble_enabled"] = true;
  doc["ble_advertising"] = true;
  doc["ble_connected"] = true;
  doc["ble_name"] = "CardputerCodex_123ABC";
  doc["ble_status_line"] = "BLE bridge connected";
  doc["active_transport"] = "hybrid";

  DeviceState state = buildState(doc.as<JsonObjectConst>());

  EXPECT_TRUE(state.ble_enabled);
  EXPECT_TRUE(state.ble_advertising);
  EXPECT_TRUE(state.ble_connected);
  EXPECT_EQ(state.ble_name, String("CardputerCodex_123ABC"));
  EXPECT_EQ(state.ble_status_line, String("BLE bridge connected"));
  EXPECT_EQ(state.active_transport, String("hybrid"));

  return g_failures == 0;
}

bool test_runtime_network_config_surface() {
  RuntimeNetworkConfig config;

  EXPECT_EQ(config.bridge_transport, String("hybrid"));
  EXPECT_TRUE(config.ble_enabled);
  EXPECT_EQ(config.ble_name, String("CardputerCodex"));
  EXPECT_TRUE(config.ble_control_only);

  return g_failures == 0;
}

bool test_buddy_app_flows() {
  BuddyApp app;
  DeviceState state;

  app.onEnter(state);
  EXPECT_EQ(state.ui_mode, UiMode::Home);
  EXPECT_EQ(state.status_line, String("Buddy mode active"));

  state.wifi_connected = true;
  state.wifi_ssid = "LabNet";
  state.wifi_ip = "10.0.0.5";
  state.bridge_status_line = "linked";
  state.codex_usage_percent = 12;
  state.codex_usage_window_minutes = 60;
  state.codex_workspace_path = "C:/repo";
  state.codex_branch = "main";
  state.codex_thread_id = "thr_1";
  state.codex_stream_line = "streaming";
  state.approval_pending = true;
  state.approval_title = "Delete build";
  state.approval_detail_line = "rm -rf build";

  FakeDisplay display;
  app.render(display, state);
  EXPECT_TRUE(contains(display.str(), "Wi-Fi   LabNet 10.0.0.5"));
  EXPECT_TRUE(contains(display.str(), "Bridge  linked"));
  EXPECT_TRUE(contains(display.str(), "Usage   12% / 60m"));
  EXPECT_TRUE(contains(display.str(), "Project C:/repo"));
  EXPECT_TRUE(contains(display.str(), "rm -rf build"));
  app.onCommand("status", state);
  EXPECT_EQ(state.status_line, String("Buddy view ready for live status"));

  app.onAction(UiAction::Select, state);
  EXPECT_EQ(state.status_line, String("Open Pager to browse sessions"));
  EXPECT_TRUE(state.activity_log_count > 0);
  EXPECT_EQ(activity_log_entry(state, state.activity_log_count - 1), String("Open Pager to browse sessions"));

  return g_failures == 0;
}

bool test_push_to_codex_flows() {
  PushToCodexApp app;
  DeviceState state;

  app.onEnter(state);
  EXPECT_EQ(state.ptt_state, PushToTalkState::Armed);
  EXPECT_EQ(state.status_line, String("Tap SPACE for a space, hold SPACE to record"));

  app.onSubmit("", state);
  EXPECT_EQ(state.status_line, String("Prompt is empty"));

  app.onTextInput("hello", false, state);
  EXPECT_EQ(state.status_line, String("Prompt staged for delivery"));

  FakeDisplay display;
  app.render(display, state);
  EXPECT_TRUE(contains(display.str(), "Draft: hello"));
  EXPECT_TRUE(contains(display.str(), "PTT   armed"));

  app.onSubmit("", state);
  EXPECT_EQ(state.status_line, String("Text prompt staged for middleware bridge"));

  app.onPushToTalk(true, state);
  EXPECT_EQ(state.ptt_state, PushToTalkState::Recording);
  EXPECT_EQ(state.status_line, String("Recording voice prompt..."));

  app.onPushToTalk(false, state);
  EXPECT_EQ(state.ptt_state, PushToTalkState::Armed);
  EXPECT_EQ(state.status_line, String("No audio captured"));

  return g_failures == 0;
}

bool test_pager_app_flows() {
  PagerApp app;
  DeviceState state;

  state.pager.session_count = 2;
  state.pager.sessions[0].session_id = "s1";
  state.pager.sessions[0].thread_id = "thr-1";
  state.pager.sessions[0].workspace_path = "C:/repo";
  state.pager.sessions[0].branch = "main";
  state.pager.sessions[0].title = "First";
  state.pager.sessions[0].status = "running";
  state.pager.sessions[1].session_id = "s2";
  state.pager.sessions[1].thread_id = "thr-2";
  state.pager.sessions[1].workspace_path = "C:/repo";
  state.pager.sessions[1].branch = "feature";
  state.pager.sessions[1].title = "Second";
  state.pager.sessions[1].status = "queued";

  app.onEnter(state);
  EXPECT_EQ(state.pager_screen, PagerScreen::Inbox);
  EXPECT_EQ(state.status_line, String("Browse sessions or compose a prompt"));

  app.onCommand("2", state);
  EXPECT_EQ(state.pager_screen, PagerScreen::Detail);
  EXPECT_EQ(state.pager.selected_session_id, String("s2"));
  EXPECT_EQ(state.status_line, String("Selected session 2"));

  app.onAction(UiAction::Back, state);
  EXPECT_EQ(state.pager_screen, PagerScreen::Inbox);

  app.onCommand("compose", state);
  EXPECT_EQ(state.pager_screen, PagerScreen::Compose);
  EXPECT_EQ(state.ui_mode, UiMode::Input);

  app.onTextInput("hello", false, state);
  EXPECT_EQ(state.status_line, String("Prompt staged"));

  app.onSubmit("", state);
  EXPECT_EQ(state.status_line, String("Prompt not sent"));
  EXPECT_EQ(state.pager_screen, PagerScreen::Compose);

  FakeDisplay display;
  app.render(display, state);
  EXPECT_TRUE(contains(display.str(), "[ COMPOSE ]"));
  EXPECT_TRUE(contains(display.str(), "To:  thr-2"));
  EXPECT_TRUE(contains(display.str(), "Draft:"));
  EXPECT_TRUE(contains(display.str(), "hello"));

  return g_failures == 0;
}

bool test_mcp_bridge_app_flows() {
  McpBridgeApp app;
  DeviceState state;

  app.onEnter(state);
  EXPECT_EQ(state.ui_mode, UiMode::Menu);
  EXPECT_EQ(state.status_line, String("Local bridge mode active"));

  app.onCommand("/notify hello", state);
  EXPECT_EQ(state.bridge_prompt_kind, BridgePromptKind::Notification);
  EXPECT_EQ(state.ui_mode, UiMode::Modal);
  EXPECT_EQ(state.status_line, String("hello"));

  app.onCommand("/confirm delete build", state);
  EXPECT_EQ(state.bridge_prompt_kind, BridgePromptKind::Confirmation);
  EXPECT_TRUE(state.bridge_prompt_pending);
  EXPECT_EQ(state.bridge_prompt_option_count, static_cast<size_t>(2));

  app.onAction(UiAction::Down, state);
  EXPECT_EQ(state.bridge_prompt_selected_index, static_cast<size_t>(1));
  EXPECT_EQ(state.status_line, String("Bridge option selected: Reject"));

  app.onAction(UiAction::Back, state);
  EXPECT_EQ(state.bridge_prompt_pending, false);
  EXPECT_EQ(state.bridge_status_line, String("Bridge prompt rejected"));
  EXPECT_EQ(state.ui_mode, UiMode::Menu);

  app.onCommand("/ask Title|Detail|Yes|No|Maybe", state);
  EXPECT_EQ(state.bridge_prompt_kind, BridgePromptKind::Question);
  EXPECT_EQ(state.bridge_prompt_option_count, static_cast<size_t>(3));

  FakeDisplay display;
  app.render(display, state);
  EXPECT_TRUE(contains(display.str(), "[ QUESTION ]"));
  EXPECT_TRUE(contains(display.str(), "Title"));
  EXPECT_TRUE(contains(display.str(), "Maybe"));

  return g_failures == 0;
}

bool test_settings_app_flows() {
  SettingsApp app;
  DeviceState state;

  app.onEnter(state);
  EXPECT_EQ(state.ui_mode, UiMode::Menu);
  EXPECT_EQ(state.status_line, String("Settings ready"));

  app.onAction(UiAction::Down, state);
  app.onAction(UiAction::Down, state);
  app.onCommand("/help", state);
  EXPECT_EQ(state.status_line, String("Use Fn+;/. Enter Del and Ctrl-M menu"));

  app.onAction(UiAction::Select, state);
  EXPECT_TRUE(contains(state.status_line, "Ctrl-M menu, Fn+;/. move, Enter select, Del back"));

  FakeDisplay display;
  app.render(display, state);
  EXPECT_TRUE(contains(display.str(), " > Keymap"));

  return g_failures == 0;
}

}  // namespace

int main() {
  std::cout.setf(std::ios::unitbuf);
  std::cerr.setf(std::ios::unitbuf);

  try {
    std::cout << "running protocol contracts\n";
    test_protocol_contracts();
    std::cout << "running protocol edge cases\n";
    test_protocol_edge_cases();
    std::cout << "running BLE state surface\n";
    test_ble_state_surface();
    std::cout << "running runtime network config surface\n";
    test_runtime_network_config_surface();
    std::cout << "running keyboard contracts\n";
    test_keyboard_contracts();
    std::cout << "running buddy app flows\n";
    test_buddy_app_flows();
    std::cout << "running push to codex flows\n";
    test_push_to_codex_flows();
    std::cout << "running pager app flows\n";
    test_pager_app_flows();
    std::cout << "running mcp bridge flows\n";
    test_mcp_bridge_app_flows();
    std::cout << "running settings app flows\n";
    test_settings_app_flows();
    std::cout << "running menu throttle\n";
    test_menu_navigation_throttle();
    std::cout << "running snapshot contracts\n";
    test_terminal_snapshots();
  } catch (const std::exception& error) {
    std::cerr << "native firmware tests crashed: " << error.what() << '\n';
    return 1;
  }

  if (g_failures == 0) {
    std::cout << "native firmware tests passed\n";
  }
  return g_failures == 0 ? 0 : 1;
}
