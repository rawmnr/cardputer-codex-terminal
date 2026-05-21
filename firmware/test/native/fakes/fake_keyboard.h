#pragma once

#include <Arduino.h>

struct FakeKeyboardState {
  String word;
  bool ctrl = false;
  bool tab = false;
  bool enter = false;
  bool del = false;
  bool space = false;
  bool fn = false;
  bool pressed = false;
  bool changed = false;
};

class FakeKeyboard {
 public:
  const FakeKeyboardState& state() const {
    return state_;
  }

  void setWord(const String& word) {
    state_.word = word;
  }

  void setFn(bool fn) {
    state_.fn = fn;
  }

  void setPressed(bool pressed) {
    state_.pressed = pressed;
  }

  void setChanged(bool changed) {
    state_.changed = changed;
  }

 private:
  FakeKeyboardState state_;
};
