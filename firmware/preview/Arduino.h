#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <stdarg.h>

#include <ArduinoJson.h>

class Print {
 public:
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t* buffer, size_t size) {
    size_t n = 0;
    while (size--) n += write(*buffer++);
    return n;
  }
  void print(char c) { write(static_cast<uint8_t>(c)); }
  void print(const std::string& s) { for (char c : s) write(c); }
  void print(const char* s) { if (s) while (*s) write(*s++); }
  void print(int n) { print(std::to_string(n)); }
  void print(unsigned int n) { print(std::to_string(n)); }
  void print(long n) { print(std::to_string(n)); }
  void print(unsigned long n) { print(std::to_string(n)); }
  void print(size_t n) { print((unsigned long)n); }
  void println(char c) { print(c); write('\n'); }
  void println(const std::string& s) { print(s); write('\n'); }
  void println(const char* s) { print(s); write('\n'); }
  void println(int n) { print(n); write('\n'); }
  void println(unsigned int n) { print(n); write('\n'); }
  void println(unsigned long n) { print(n); write('\n'); }
  void println(size_t n) { println((unsigned long)n); }
  void println() { write('\n'); }
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

  bool startsWith(const String& prefix) const {
    return length() >= prefix.length() && compare(0, prefix.length(), prefix) == 0;
  }

  bool endsWith(const String& suffix) const {
    return length() >= suffix.length() && compare(length() - suffix.length(), suffix.length(), suffix) == 0;
  }

  void trim() {
    size_t first = find_first_not_of(" \t\r\n");
    if (first == npos) {
      clear();
      return;
    }
    size_t last = find_last_not_of(" \t\r\n");
    *this = substr(first, (last - first + 1));
  }

  int toInt() const {
    try { return std::stoi(*this); } catch (...) { return 0; }
  }

  void remove(size_t index, size_t count = npos) {
    if (index < length()) erase(index, count);
  }

  int indexOf(char c, size_t from = 0) const {
    size_t res = find(c, from);
    return res == npos ? -1 : (int)res;
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

#ifndef StringPrint_h
#define StringPrint_h
class StringPrint : public Print {
 public:
  std::stringstream ss;
  size_t write(uint8_t c) override { ss << (char)c; return 1; }
  std::string str() const { return ss.str(); }
};
#endif

class SerialMock : public Print {
 public:
  size_t write(uint8_t c) override { std::cout << (char)c; return 1; }
};
extern SerialMock Serial;

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

typedef int32_t esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1

#ifndef WiFi_h
#define WiFi_h
enum wl_status_t {
  WL_NO_SHIELD = 255,
  WL_IDLE_STATUS = 0,
  WL_NO_SSID_AVAIL = 1,
  WL_SCAN_COMPLETED = 2,
  WL_CONNECTED = 3,
  WL_CONNECT_FAILED = 4,
  WL_CONNECTION_LOST = 5,
  WL_DISCONNECTED = 6
};

class IPAddress {
 public:
  String toString() const { return "127.0.0.1"; }
};

class WiFiClass {
 public:
  void begin(const char*, const char*) {}
  wl_status_t status();
  IPAddress localIP() { return IPAddress(); }
  String SSID() { return "MockWiFi"; }
  void disconnect() {}
  void disconnect(bool, bool) {}
  void mode(int) {}
  void persistent(bool) {}
  void setHostname(const char*) {}
  void setAutoReconnect(bool) {}
  void setSleep(bool) {}
};
extern WiFiClass WiFi;
#define WIFI_STA 1
#endif

#ifndef WebSocketsClient_h
#define WebSocketsClient_h
enum WStype_t {
  WStype_DISCONNECTED,
  WStype_CONNECTED,
  WStype_TEXT,
  WStype_BIN,
  WStype_ERROR,
  WStype_FRAGMENT_TEXT_START,
  WStype_FRAGMENT_BIN_START,
  WStype_FRAGMENT,
  WStype_FRAGMENT_FIN,
  WStype_PING,
  WStype_PONG,
};

class WebSocketsClient {
 public:
  void begin(const char*, uint16_t, const char* = "/", const char* = "ws") {}
  void onEvent(void (*)(WStype_t, uint8_t*, size_t)) {}
  void setReconnectInterval(uint32_t) {}
  void loop() {}
  bool sendTXT(String&) { return true; }
  bool sendTXT(const char*) { return true; }
  bool isConnected() { return false; }
  void enableHeartbeat(uint32_t, uint32_t, uint8_t) {}
};
#endif

#ifndef HTTPClient_h
#define HTTPClient_h
class HTTPClient {
 public:
  void begin(const String&) {}
  int GET() { return 200; }
  String getString() { return "{}"; }
  void end() {}
};
class WiFiClient {
 public:
  bool connect(const char*, uint16_t) { return true; }
  void stop() {}
};
#endif

#ifndef SD_h
#define SD_h
#define FILE_READ 0
#define FILE_WRITE 1
#define FILE_APPEND 2

class File : public Print {
 public:
  operator bool() const { return false; }
  size_t write(uint8_t) override { return 0; }
  bool available() { return false; }
  String readStringUntil(char) { return ""; }
  void close() {}
};

class SDClass {
 public:
  template<typename... Args>
  bool begin(Args...) { return true; }
  bool exists(const char*) { return false; }
  File open(const char*, int = FILE_READ) { return File(); }
  void mkdir(const char*) {}
};
extern SDClass SD;
#endif

#ifndef ESPmDNS_h
#define ESPmDNS_h
class MDNSClass {
 public:
  bool begin(const char*) { return true; }
};
extern MDNSClass MDNS;
#endif

#ifndef SPI_h
#define SPI_h
class SPIClass {
 public:
  void begin(int, int, int, int) {}
};
extern SPIClass SPI;
#endif
