#include "protocol.h"

#include <string>

namespace {
constexpr size_t kJsonCapacity = 2048;

class StringWriter {
 public:
  size_t write(uint8_t c) {
    output_.push_back(static_cast<char>(c));
    return 1;
  }

  size_t write(const uint8_t* buffer, size_t size) {
    output_.append(reinterpret_cast<const char*>(buffer), size);
    return size;
  }

  String take() const {
    return String(output_.c_str());
  }

 private:
  std::string output_;
};
}  // namespace

String buildCardputerEnvelope(const String& id, const String& type, const JsonVariantConst& payload, const String& auth_token) {
  DynamicJsonDocument doc(kJsonCapacity);
  doc["protocol_version"] = kCardputerProtocolVersion;
  doc["id"] = id;
  doc["type"] = type;
  doc["payload"] = payload;
  if (auth_token.length() > 0) {
    doc["auth_token"] = auth_token;
  }

  StringWriter writer;
  serializeJson(doc, writer);
  return writer.take();
}
