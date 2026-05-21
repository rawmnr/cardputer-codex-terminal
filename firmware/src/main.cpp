#include <Arduino.h>
#include <M5Cardputer.h>

#include "app_shell.h"
#include "input_router.h"

#if USE_LVGL_UI
#include <lvgl.h>
#endif

namespace {
AppShell g_shell;
bool g_prev_space_state = false;
bool g_prev_tab_state = false;
bool g_prev_enter_state = false;
bool g_prev_del_state = false;
bool g_prev_ctrl_state = false;
String g_prev_word;
bool g_space_hold_started = false;
unsigned long g_space_pressed_at_ms = 0;
unsigned long g_last_menu_nav_ms = 0;
constexpr unsigned long kPushToTalkHoldMs = 350;

#if USE_LVGL_UI
void dispatchAction(UiAction action) {
  if (action == UiAction::None) {
    return;
  }

  const bool menu_navigation = g_shell.isAppMenuOpen() && (action == UiAction::Up || action == UiAction::Down);
  if (menu_navigation) {
    const unsigned long now = millis();
    if (!shouldAllowMenuNavigation(now, g_last_menu_nav_ms)) {
      return;
    }
    g_last_menu_nav_ms = now;
  }

  g_shell.handleAction(action);
}
#else
void dispatchAction(UiAction action) {
  g_shell.handleAction(action);
}
#endif


void sendTypedChar(char ch) {
  String typed;
  typed += ch;
  g_shell.handleTextInput(typed, false, false);
}

void poll_keyboard_input() {
  M5Cardputer.update();

  const auto status = M5Cardputer.Keyboard.keysState();
  const bool push_mode = g_shell.isPushToCodexActive();
  const bool input_mode = g_shell.uiMode() == UiMode::Input;
  String typed;
  for (size_t i = 0; i < status.word.size(); ++i) {
    const char ch = status.word[i];
    if (push_mode && status.space && ch == ' ') {
      continue;
    }
    typed += ch;
  }
  const bool tab_pressed = status.tab && !g_prev_tab_state;
  const bool enter_pressed = status.enter && !g_prev_enter_state;
  const bool del_pressed = status.del && !g_prev_del_state;
  const bool ctrl_m_pressed =
    status.ctrl &&
    typed.length() == 1 &&
    (typed[0] == 'm' || typed[0] == 'M') &&
    !(g_prev_ctrl_state && g_prev_word == typed);

  if (M5Cardputer.Keyboard.isChange()) {
    g_shell.noteInteraction();
  }

  if (push_mode && status.space && !g_space_hold_started && g_space_pressed_at_ms > 0) {
    const unsigned long held_ms = millis() - g_space_pressed_at_ms;
    if (held_ms >= kPushToTalkHoldMs) {
      g_shell.handleAction(UiAction::PushToTalkStart);
      g_space_hold_started = true;
    }
  }

  if (tab_pressed) {
    if (!input_mode && !push_mode && !g_shell.hasVisibleModal()) {
#if USE_LVGL_UI
      if (g_shell.isAppMenuOpen()) {
        dispatchAction(UiAction::Down);
      } else {
        dispatchAction(UiAction::Menu);
      }
#else
      dispatchAction(UiAction::Menu);
#endif
    }
    g_prev_tab_state = status.tab;
    g_prev_enter_state = status.enter;
    g_prev_del_state = status.del;
    g_prev_ctrl_state = status.ctrl;
    g_prev_word = typed;
    return;
  }

  if (ctrl_m_pressed) {
    dispatchAction(UiAction::Menu);
    g_prev_tab_state = status.tab;
    g_prev_enter_state = status.enter;
    g_prev_del_state = status.del;
    g_prev_ctrl_state = status.ctrl;
    g_prev_word = typed;
    return;
  }

  if (enter_pressed) {
    if (input_mode || push_mode) {
      g_shell.handleTextInput("", true, false);
    } else {
      dispatchAction(UiAction::Select);
    }
    g_prev_tab_state = status.tab;
    g_prev_enter_state = status.enter;
    g_prev_del_state = status.del;
    g_prev_ctrl_state = status.ctrl;
    g_prev_word = typed;
    return;
  }

  if (del_pressed) {
    if (input_mode || push_mode) {
      g_shell.handleTextInput("", false, true);
    } else {
      dispatchAction(UiAction::Back);
    }
    g_prev_tab_state = status.tab;
    g_prev_enter_state = status.enter;
    g_prev_del_state = status.del;
    g_prev_ctrl_state = status.ctrl;
    g_prev_word = typed;
    return;
  }

  if (!status.space && g_prev_space_state) {
    if (push_mode && g_space_hold_started) {
      g_shell.handleAction(UiAction::PushToTalkStop);
    } else if (input_mode || push_mode) {
      g_shell.handleTextInput(" ", false, false);
    } else {
      dispatchAction(UiAction::Select);
    }
    g_space_pressed_at_ms = 0;
    g_space_hold_started = false;
  } else if (status.space && !g_prev_space_state) {
    g_space_pressed_at_ms = millis();
    g_space_hold_started = false;
  }

  if (!M5Cardputer.Keyboard.isPressed()) {
    g_prev_space_state = status.space;
    g_prev_tab_state = status.tab;
    g_prev_enter_state = status.enter;
    g_prev_del_state = status.del;
    g_prev_ctrl_state = status.ctrl;
    g_prev_word = typed;
    if (push_mode && status.space && !g_space_hold_started && g_space_pressed_at_ms > 0) {
      const unsigned long held_ms = millis() - g_space_pressed_at_ms;
      if (held_ms >= kPushToTalkHoldMs) {
        g_shell.handleAction(UiAction::PushToTalkStart);
        g_space_hold_started = true;
      }
    }
    return;
  }

  if (typed.length() > 0) {
    if (input_mode || push_mode) {
      g_shell.handleTextInput(typed, false, false);
    } else {
      for (size_t i = 0; i < typed.length(); ++i) {
        const char ch = typed[i];
        UiAction action = UiAction::None;
        if (mapNavigationChar(ch, status.fn, action)) {
          dispatchAction(action);
        } else {
          sendTypedChar(ch);
        }
      }
    }
  }

  g_prev_space_state = status.space;
  g_prev_tab_state = status.tab;
  g_prev_enter_state = status.enter;
  g_prev_del_state = status.del;
  g_prev_ctrl_state = status.ctrl;
  g_prev_word = typed;
}
}  // namespace

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  Serial.begin(115200);
  delay(200);

  g_shell.begin();
  g_shell.render();

  Serial.println();
  Serial.println("cardputer-codex-terminal firmware shell ready");
  Serial.println("Type /help for commands.");
}

void loop() {
  poll_keyboard_input();
  g_shell.tick();
  delay(16);
}
