#pragma once
#include <cstdint>

namespace board {
class DebouncedButton {
 public:
  void begin(bool pressed, uint32_t now) {
    raw_ = stable_ = pressed;
    changedAt_ = now;
    armed_ = !pressed;
  }
  // Returns one press event after 40 ms stable LOW; held at boot must release.
  bool update(bool pressed, uint32_t now) {
    if (pressed != raw_) { raw_ = pressed; changedAt_ = now; }
    if (raw_ == stable_ || static_cast<uint32_t>(now - changedAt_) < 40) return false;
    stable_ = raw_;
    if (!stable_) { armed_ = true; return false; }
    const bool event = armed_;
    armed_ = false;
    return event;
  }
 private:
  bool raw_ = false, stable_ = false, armed_ = true;
  uint32_t changedAt_ = 0;
};
}
