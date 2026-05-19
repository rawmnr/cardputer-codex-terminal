#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include <algorithm>

class String : public std::string {
 public:
  using std::string::string;
  String(const std::string& s) : std::string(s) {}
  String(const char* s) : std::string(s ? s : "") {}
  String(int n) : std::string(std::to_string(n)) {}
  String(unsigned int n) : std::string(std::to_string(n)) {}
  String(long n) : std::string(std::to_string(n)) {}
  String(unsigned long n) : std::string(std::to_string(n)) {}
  String(float n) : std::string(std::to_string(n)) {}

  const char* c_str() const { return std::string::c_str(); }
  size_t length() const { return std::string::length(); }

  String substring(size_t from, size_t to = -1) const {
    if (to == (size_t)-1) to = length();
    if (from >= length()) return "";
    if (to <= from) return "";
    return std::string::substr(from, to - from);
  }

  bool startsWith(const String& prefix) const {
    if (prefix.length() > length()) return false;
    return std::string::substr(0, prefix.length()) == prefix;
  }

  void toLowerCase() {
    std::transform(begin(), end(), begin(), [](unsigned char c) { return std::tolower(c); });
  }

  int toInt() const {
    try {
      return std::stoi(*this);
    } catch (...) {
      return 0;
    }
  }

  String& operator+=(const String& other) {
    std::string::operator+=(other);
    return *this;
  }

  String& operator+=(const char* other) {
    std::string::operator+=(other);
    return *this;
  }

  String& operator+=(char c) {
    std::string::push_back(c);
    return *this;
  }
};

inline String operator+(String lhs, const String& rhs) {
  lhs += rhs;
  return lhs;
}

inline String operator+(String lhs, const char* rhs) {
  lhs += rhs;
  return lhs;
}

inline String operator+(const char* lhs, const String& rhs) {
  String s(lhs);
  s += rhs;
  return s;
}

inline uint32_t millis() {
  static auto start = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
}

#define PROGMEM
#define PSTR(s) (s)
#define F(s) (s)
