#pragma once
#include <ArduinoJson.h>
#include <cstddef>

namespace display {
// ArduinoJson stops after one value; reject trailing documents/data explicitly.
inline bool readJsonObject(const char* json, size_t length, size_t limit,
                           JsonDocument& document) {
  if (!json || !length || length > limit) return false;
  struct Reader {
    const char* text;
    size_t size, position = 0;
    Reader(const char* input, size_t count) : text(input), size(count) {}
    int read() { return position < size ? static_cast<unsigned char>(text[position++]) : -1; }
  } reader(json, length);
  if (deserializeJson(document, reader, DeserializationOption::NestingLimit(3)) ||
      !document.is<JsonObject>()) return false;
  for (; reader.position < length; ++reader.position) {
    const char c = json[reader.position];
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') return false;
  }
  return true;
}
}  // namespace display
