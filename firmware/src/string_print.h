#pragma once

#include <Arduino.h>

class StringPrint : public Print {
 public:
  StringPrint() = default;

  using Print::write;

  size_t write(uint8_t value) override {
    buffer_ += static_cast<char>(value);
    return 1;
  }

  size_t write(const uint8_t* buffer, size_t size) override {
    for (size_t i = 0; i < size; ++i) {
      buffer_ += static_cast<char>(buffer[i]);
    }
    return size;
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
