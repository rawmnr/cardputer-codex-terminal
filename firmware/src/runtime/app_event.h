#pragma once

#include <Arduino.h>

#include <array>
#include <cstdint>
#include <cstring>

#include "ui_actions.h"

struct AppEvent {
  static constexpr size_t kTextCapacity = 96;

  enum class Type : uint8_t {
    None = 0,
    Boot,
    KeyboardKey,
    UiAction,
    TextInput,
    PushToTalk,
    Status,
    Log,
    BridgeJson,
    DiagnosticTick,
    BleStatus,
  };

  enum class KeyAction : uint8_t {
    Press = 0,
    Release = 1,
    Repeat = 2,
  };

  Type type = Type::None;
  uint32_t timestamp_ms = 0;
  UiAction ui_action = UiAction::None;
  bool pressed = false;
  KeyAction key_action = KeyAction::Press;
  uint8_t key_repeat = 0;
  bool submit = false;
  bool backspace = false;
  bool truncated = false;
  uint16_t value = 0;
  uint16_t text_length = 0;
  char text[kTextCapacity]{};

  static AppEvent boot(uint32_t timestamp_ms) {
    AppEvent event;
    event.type = Type::Boot;
    event.timestamp_ms = timestamp_ms;
    return event;
  }

  static AppEvent keyboard(uint32_t timestamp_ms, uint16_t key_code, bool key_pressed, bool repeated = false) {
    AppEvent event;
    event.type = Type::KeyboardKey;
    event.timestamp_ms = timestamp_ms;
    event.value = key_code;
    event.pressed = key_pressed;
    event.key_action = repeated ? KeyAction::Repeat : (key_pressed ? KeyAction::Press : KeyAction::Release);
    event.key_repeat = repeated ? 1 : 0;
    return event;
  }

  static AppEvent action(uint32_t timestamp_ms, UiAction ui_action) {
    AppEvent event;
    event.type = Type::UiAction;
    event.timestamp_ms = timestamp_ms;
    event.ui_action = ui_action;
    return event;
  }

  static AppEvent textInput(uint32_t timestamp_ms, const String& typed, bool should_submit, bool should_backspace) {
    AppEvent event;
    event.type = Type::TextInput;
    event.timestamp_ms = timestamp_ms;
    event.submit = should_submit;
    event.backspace = should_backspace;
    event.assignText(typed.c_str(), typed.length());
    return event;
  }

  static AppEvent textInput(uint32_t timestamp_ms, const char* typed, bool should_submit, bool should_backspace) {
    AppEvent event;
    event.type = Type::TextInput;
    event.timestamp_ms = timestamp_ms;
    event.submit = should_submit;
    event.backspace = should_backspace;
    event.assignText(typed, typed != nullptr ? strlen(typed) : 0);
    return event;
  }

  static AppEvent pushToTalk(uint32_t timestamp_ms, bool key_pressed) {
    AppEvent event;
    event.type = Type::PushToTalk;
    event.timestamp_ms = timestamp_ms;
    event.pressed = key_pressed;
    return event;
  }

  static AppEvent status(uint32_t timestamp_ms, const char* message) {
    AppEvent event;
    event.type = Type::Status;
    event.timestamp_ms = timestamp_ms;
    event.assignText(message, message != nullptr ? strlen(message) : 0);
    return event;
  }

  static AppEvent log(uint32_t timestamp_ms, const char* message) {
    AppEvent event;
    event.type = Type::Log;
    event.timestamp_ms = timestamp_ms;
    event.assignText(message, message != nullptr ? strlen(message) : 0);
    return event;
  }

  static AppEvent bridgeJson(uint32_t timestamp_ms, const char* payload, size_t length) {
    AppEvent event;
    event.type = Type::BridgeJson;
    event.timestamp_ms = timestamp_ms;
    event.assignText(payload, length);
    return event;
  }

  static AppEvent diagnosticTick(uint32_t timestamp_ms) {
    AppEvent event;
    event.type = Type::DiagnosticTick;
    event.timestamp_ms = timestamp_ms;
    return event;
  }
  static AppEvent bleStatus(uint32_t timestamp_ms, bool connected, bool advertising, const char* message) {
    AppEvent event;
    event.type = Type::BleStatus;
    event.timestamp_ms = timestamp_ms;
    event.pressed = connected;
    event.submit = advertising;
    event.assignText(message, message != nullptr ? strlen(message) : 0);
    return event;
  }

  bool isLowPriority() const {
    return type == Type::Log || type == Type::Status || type == Type::DiagnosticTick;
  }

  void assignText(const char* source, size_t length) {
    if (source == nullptr || length == 0) {
      text[0] = '\0';
      text_length = 0;
      truncated = false;
      return;
    }

    const size_t copied = length < (kTextCapacity - 1) ? length : (kTextCapacity - 1);
    if (copied > 0) {
      memcpy(text, source, copied);
    }
    text[copied] = '\0';
    text_length = static_cast<uint16_t>(copied);
    truncated = copied < length;
  }

  String asString() const { return String(text); }
};

template <size_t Capacity>
class AppEventQueue {
 public:
  bool push(const AppEvent& event) {
    // Reserve space for high-priority events (UI actions, keyboard)
    constexpr size_t kReserved = 8;
    if (size_ >= Capacity || (event.isLowPriority() && size_ >= (Capacity - kReserved))) {
      ++dropped_;
      return false;
    }

    events_[tail_] = event;
    tail_ = (tail_ + 1) % Capacity;
    size_++;
    return true;
  }


  bool pop(AppEvent& event) {
    if (size_ == 0) {
      return false;
    }

    event = events_[head_];
    head_ = (head_ + 1) % Capacity;
    --size_;
    return true;
  }

  void clear() {
    head_ = 0;
    tail_ = 0;
    size_ = 0;
  }

  size_t size() const { return size_; }
  constexpr size_t capacity() const { return Capacity; }
  uint32_t droppedCount() const { return dropped_; }

 private:
  std::array<AppEvent, Capacity> events_{};
  size_t head_ = 0;
  size_t tail_ = 0;
  size_t size_ = 0;
  uint32_t dropped_ = 0;
};