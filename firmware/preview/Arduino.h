#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>

class Print {
 public:
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t* buffer, size_t size) {
    size_t n = 0;
    while (size--) n += write(*buffer++);
    return n;
  }
};

class String : public std::string {
 public:
  using std::string::string;
  String() = default;
  String(const char* s) : std::string(s ? s : "") {}
  String(const std::string& s) : std::string(s) {}
  String(int n) : std::string(std::to_string(n)) {}
  String(unsigned int n) : std::string(std::to_string(n)) {}
  String(long n) : std::string(std::to_string(n)) {}
  String(unsigned long n) : std::string(std::to_string(n)) {}
  String(long long n) : std::string(std::to_string(n)) {}
  String(unsigned long long n) : std::string(std::to_string(n)) {}
  String(float n) : std::string(std::to_string(n)) {}
  String(double n) : std::string(std::to_string(n)) {}

  const char* c_str() const { return std::string::c_str(); }
  size_t length() const { return std::string::length(); }

  String substr(size_t from, size_t n = std::string::npos) const {
    return String(std::string::substr(from, n));
  }

  String substring(size_t from, size_t to = std::string::npos) const {
    if (from >= length()) return String("");
    if (to == std::string::npos) return String(substr(from));
    if (to <= from) return String("");
    return String(substr(from, to - from));
  }

  String operator+(const String& other) const {
    String res = *this;
    res.append(other);
    return res;
  }

  String operator+(const char* other) const {
    String res = *this;
    res.append(other ? other : "");
    return res;
  }

  String operator+(char other) const {
    String res = *this;
    res.append(1, other);
    return res;
  }

  String& operator+=(const String& other) {
    this->append(other);
    return *this;
  }

  String& operator+=(const char* other) {
    this->append(other ? other : "");
    return *this;
  }

  friend String operator+(const char* a, const String& b) {
    return String(a) + b;
  }
};

inline int min(int a, int b) { return std::min(a, b); }
inline size_t min(size_t a, size_t b) { return std::min(a, b); }
inline int max(int a, int b) { return std::max(a, b); }
inline size_t max(size_t a, size_t b) { return std::max(a, b); }
inline int constrain(int x, int a, int b) { return std::max(a, std::min(x, b)); }

inline uint32_t millis() {
  auto now = std::chrono::steady_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
  return static_cast<uint32_t>(ms.count());
}

inline void delay(uint32_t ms) {
  // no-op for preview
}
