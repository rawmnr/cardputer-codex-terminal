#pragma once

class FakeClock {
 public:
  unsigned long now() const {
    return now_ms_;
  }

  void set(unsigned long now_ms) {
    now_ms_ = now_ms;
  }

  void advance(unsigned long delta_ms) {
    now_ms_ += delta_ms;
  }

 private:
  unsigned long now_ms_ = 0;
};
