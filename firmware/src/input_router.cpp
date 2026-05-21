#include "input_router.h"

bool shouldAllowMenuNavigation(unsigned long now_ms, unsigned long last_menu_nav_ms) {
  return now_ms - last_menu_nav_ms >= kMenuNavRepeatMs;
}

bool mapNavigationChar(char ch, bool fn, UiAction& action) {
  if (fn && ch == ';') {
    action = UiAction::Up;
    return true;
  }
  if (fn && ch == '.') {
    action = UiAction::Down;
    return true;
  }

  switch (ch) {
    case 'a':
    case 'A':
    case ',':
    case ';':
      action = UiAction::Left;
      return true;
    case 'd':
    case 'D':
    case '.':
    case '\'':
      action = UiAction::Right;
      return true;
    default:
      return false;
  }
}
