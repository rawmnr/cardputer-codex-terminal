#pragma once

#include <Arduino.h>
#include <atomic>

#if defined(ARDUINO) && !defined(NATIVE_BUILD)
#include <M5Cardputer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#endif

#include "app_event.h"
#include "app_shell.h"
#include "input_router.h"
#include "resource_guard.h"

#if defined(ARDUINO) && !defined(NATIVE_BUILD)
class FirmwareRuntime {
 public:
  void begin() {
    ResourceGuard::begin();
    if (shell_mutex_ == nullptr) {
      shell_mutex_ = xSemaphoreCreateMutex();
    }
    if (event_mutex_ == nullptr) {
      event_mutex_ = xSemaphoreCreateMutex();
    }

    shell_.begin();
    lockShell([&]() { shell_.render(); });

    xTaskCreate(uiTaskEntry, "cardputer-ui", 8192, this, 3, &ui_task_);
    xTaskCreate(backgroundTaskEntry, "cardputer-net", 8192, this, 2, &background_task_);
    xTaskCreate(keyboardTaskEntry, "cardputer-keyboard", 6144, this, 4, &keyboard_task_);
  }

  void loop() { vTaskDelay(pdMS_TO_TICKS(250)); }

 private:
  struct ShellLock {
    SemaphoreHandle_t handle = nullptr;
    bool acquired = false;

    explicit ShellLock(SemaphoreHandle_t mutex) : handle(mutex) {
      if (handle != nullptr) {
        acquired = xSemaphoreTake(handle, portMAX_DELAY) == pdTRUE;
      }
    }

    ~ShellLock() {
      if (acquired && handle != nullptr) {
        xSemaphoreGive(handle);
      }
    }

    explicit operator bool() const { return acquired; }
  };

  struct EventLock {
    SemaphoreHandle_t handle = nullptr;
    bool acquired = false;

    explicit EventLock(SemaphoreHandle_t mutex) : handle(mutex) {
      if (handle != nullptr) {
        acquired = xSemaphoreTake(handle, portMAX_DELAY) == pdTRUE;
      }
    }

    ~EventLock() {
      if (acquired && handle != nullptr) {
        xSemaphoreGive(handle);
      }
    }

    explicit operator bool() const { return acquired; }
  };

  static FirmwareRuntime* instanceFrom(void* self) { return static_cast<FirmwareRuntime*>(self); }

  static void uiTaskEntry(void* self) { instanceFrom(self)->uiTask(); }
  static void backgroundTaskEntry(void* self) { instanceFrom(self)->backgroundTask(); }
  static void keyboardTaskEntry(void* self) { instanceFrom(self)->keyboardTask(); }

  template <typename Fn>
  void lockShell(Fn&& fn) {
    ShellLock lock(shell_mutex_);
    if (lock) {
      fn();
    }
  }

  void postEvent(const AppEvent& event) {
    EventLock lock(event_mutex_);
    if (!lock) {
      ++dropped_events_;
      return;
    }

    if (!events_.push(event)) {
      ++dropped_events_;
    }
  }

  bool popEvent(AppEvent& event) {
    EventLock lock(event_mutex_);
    if (!lock) {
      return false;
    }
    return events_.pop(event);
  }

  void uiTask() {
    for (;;) {
      AppEvent event;
      while (popEvent(event)) {
        lockShell([&]() { handleEvent(event); });
      }

      lockShell([&]() {
        shell_.tickUi();
        shell_.render();
      });

      vTaskDelay(pdMS_TO_TICKS(16));
    }
  }

  void backgroundTask() {
    for (;;) {
      // backgroundTask should perform work and then lock shell only for state application.
      // For now, we keep the lock but reduce its scope if possible.
      // tickBackground mostly handles bridge polling which can be slow.
      lockShell([&]() { shell_.tickBackground(); });

      sampleMetrics();
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }


  void keyboardTask() {
    for (;;) {
      pollKeyboard();
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }

  void handleEvent(const AppEvent& event) {
    switch (event.type) {
      case AppEvent::Type::UiAction:
        shell_.handleAction(event.ui_action);
        break;
      case AppEvent::Type::TextInput:
        shell_.handleTextInput(String(event.text), event.submit, event.backspace);
        break;
      case AppEvent::Type::PushToTalk:
        shell_.handlePushToTalk(event.pressed);
        break;
      case AppEvent::Type::BleStatus:
        shell_.handleBleStatus(event.pressed, event.submit, event.text);
        break;

      case AppEvent::Type::KeyboardKey:
#if USE_LVGL_UI
        shell_.handleUiKey(static_cast<lv_key_t>(event.value), event.pressed);
#else
        (void)event;
#endif
        break;
      case AppEvent::Type::Status:
        shell_.handleCommand(String(event.text));
        break;
      case AppEvent::Type::Log:
        Serial.println(event.text);
        break;
      case AppEvent::Type::Boot:
      case AppEvent::Type::BridgeJson:
      case AppEvent::Type::DiagnosticTick:
      case AppEvent::Type::None:
        break;
    }
  }

  void pollKeyboard() {
    const ResourceGuard::ScopedLock i2c_guard(ResourceGuard::Kind::I2c, 2);
    if (!i2c_guard.acquired()) {
      return;
    }

    M5Cardputer.update();
    const auto status = M5Cardputer.Keyboard.keysState();

    bool push_mode = false;
    bool input_mode = false;
    bool app_menu_open = false;
    bool visible_modal = false;
    lockShell([&]() {
      push_mode = shell_.isPushToCodexActive();
      input_mode = shell_.uiMode() == UiMode::Input;
      app_menu_open = shell_.isAppMenuOpen();
      visible_modal = shell_.hasVisibleModal();
    });

    String typed;
    for (size_t i = 0; i < status.word.size(); ++i) {
      const char ch = status.word[i];
      if (push_mode && status.space && ch == ' ') {
        continue;
      }
      typed += ch;
    }

    const bool tab_pressed = status.tab && !prev_tab_state_;
    const bool enter_pressed = status.enter && !prev_enter_state_;
    const bool del_pressed = status.del && !prev_del_state_;
    const bool ctrl_m_pressed =
      status.ctrl &&
      typed.length() == 1 &&
      (typed[0] == 'm' || typed[0] == 'M') &&
      !(prev_ctrl_state_ && prev_word_ == typed);

    if (M5Cardputer.Keyboard.isChange()) {
      postEvent(AppEvent::diagnosticTick(millis()));
    }

    if (push_mode && status.space && !space_hold_started_ && space_pressed_at_ms_ > 0) {
      const unsigned long held_ms = millis() - space_pressed_at_ms_;
      if (held_ms >= kPushToTalkHoldMs) {
        postEvent(AppEvent::pushToTalk(millis(), true));
        space_hold_started_ = true;
      }

    }

    if (tab_pressed) {
      if (!input_mode && !push_mode && !visible_modal) {
        if (app_menu_open) {
          postEvent(AppEvent::action(millis(), UiAction::Down));
        } else {
          postEvent(AppEvent::action(millis(), UiAction::Menu));
        }
      }
      commitKeyboardState(status, typed);
      return;
    }

    if (ctrl_m_pressed) {
      postEvent(AppEvent::action(millis(), UiAction::Menu));
      commitKeyboardState(status, typed);
      return;
    }

    if (enter_pressed) {
      if (input_mode || push_mode) {
        postEvent(AppEvent::textInput(millis(), "", true, false));
      } else {
        postEvent(AppEvent::action(millis(), UiAction::Select));
      }
      commitKeyboardState(status, typed);
      return;
    }

    if (del_pressed) {
      if (input_mode || push_mode) {
        postEvent(AppEvent::textInput(millis(), "", false, true));
      } else {
        postEvent(AppEvent::action(millis(), UiAction::Back));
      }
      commitKeyboardState(status, typed);
      return;
    }

    if (!status.space && prev_space_state_) {
      if (push_mode && space_hold_started_) {
        postEvent(AppEvent::pushToTalk(millis(), false));

      } else if (input_mode || push_mode) {
        postEvent(AppEvent::textInput(millis(), " ", false, false));
      } else {
        postEvent(AppEvent::action(millis(), UiAction::Select));
      }
      space_pressed_at_ms_ = 0;
      space_hold_started_ = false;
    } else if (status.space && !prev_space_state_) {
      space_pressed_at_ms_ = millis();
      space_hold_started_ = false;
    }

    if (!M5Cardputer.Keyboard.isPressed()) {
      commitKeyboardState(status, typed);
      if (push_mode && status.space && !space_hold_started_ && space_pressed_at_ms_ > 0) {
        const unsigned long held_ms = millis() - space_pressed_at_ms_;
        if (held_ms >= kPushToTalkHoldMs) {
          postEvent(AppEvent::pushToTalk(millis(), true));
          space_hold_started_ = true;
        }
      }
      return;
    }

    if (typed.length() > 0) {
      if (input_mode || push_mode) {
        postEvent(AppEvent::textInput(millis(), typed, false, false));
      } else {
        for (size_t i = 0; i < typed.length(); ++i) {
          const char ch = typed[i];
          UiAction action = UiAction::None;
          if (mapNavigationChar(ch, status.fn, action)) {
            postEvent(AppEvent::action(millis(), action));
          } else {
            char single[2] = {ch, '\0'};
            postEvent(AppEvent::textInput(millis(), single, false, false));
          }
        }
      }
    }

    commitKeyboardState(status, typed);
  }

  template <typename KeyboardState>
  void commitKeyboardState(const KeyboardState& status, const String& typed) {
    prev_space_state_ = status.space;
    prev_tab_state_ = status.tab;
    prev_enter_state_ = status.enter;
    prev_del_state_ = status.del;
    prev_ctrl_state_ = status.ctrl;
    prev_word_ = typed;
  }

  void sampleMetrics() {
    const unsigned long now = millis();
    if (now - last_metrics_log_ms_ < 10000) {
      return;
    }

    last_metrics_log_ms_ = now;
    const ResourceGuard::Stats guard_stats = ResourceGuard::stats();
    const uint32_t free_heap = ESP.getFreeHeap();

    EventLock lock(event_mutex_);
    const size_t queue_size = lock ? events_.size() : 0;
    const uint32_t dropped = dropped_events_.load();

    Serial.print("[runtime]");
    Serial.print(" heap=");
    Serial.print(free_heap);
    Serial.print(" drops=");
    Serial.print(dropped);
    Serial.print(" queue=");
    Serial.print(queue_size);
    Serial.print(" spi_to=");
    Serial.print(guard_stats.spi_timeouts);
    Serial.print(" i2c_to=");
    Serial.print(guard_stats.i2c_timeouts);
    Serial.print(" ui_stack=");
    Serial.print(ui_task_ != nullptr ? uxTaskGetStackHighWaterMark(ui_task_) : 0);
    Serial.print(" bg_stack=");
    Serial.print(background_task_ != nullptr ? uxTaskGetStackHighWaterMark(background_task_) : 0);
    Serial.print(" kb_stack=");
    Serial.println(keyboard_task_ != nullptr ? uxTaskGetStackHighWaterMark(keyboard_task_) : 0);
  }

  static constexpr unsigned long kPushToTalkHoldMs = 350;

  AppShell shell_;
  AppEventQueue<64> events_;
  SemaphoreHandle_t shell_mutex_ = nullptr;
  SemaphoreHandle_t event_mutex_ = nullptr;
  TaskHandle_t ui_task_ = nullptr;
  TaskHandle_t background_task_ = nullptr;
  TaskHandle_t keyboard_task_ = nullptr;

  bool prev_space_state_ = false;
  bool prev_tab_state_ = false;
  bool prev_enter_state_ = false;
  bool prev_del_state_ = false;
  bool prev_ctrl_state_ = false;
  bool space_hold_started_ = false;
  unsigned long space_pressed_at_ms_ = 0;
  unsigned long last_metrics_log_ms_ = 0;
  std::atomic<uint32_t> dropped_events_{0};
  String prev_word_;
};
#else
class FirmwareRuntime {
 public:
  void begin() {}
  void loop() {}
};
#endif
