#pragma once

#include <Arduino.h>

#include <string>

class CapturedPrint : public Print {
 public:
  size_t write(uint8_t c) override {
    buffer_.push_back(static_cast<char>(c));
    return 1;
  }

  const String& str() const {
    return buffer_;
  }

  void clear() {
    buffer_ = "";
  }

 private:
  String buffer_;
};
