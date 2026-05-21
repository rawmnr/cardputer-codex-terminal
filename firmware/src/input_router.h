#pragma once

#include <Arduino.h>

#include "ui_actions.h"

constexpr unsigned long kMenuNavRepeatMs = 30;

bool shouldAllowMenuNavigation(unsigned long now_ms, unsigned long last_menu_nav_ms);
bool mapNavigationChar(char ch, bool fn, UiAction& action);
