#pragma once

#include <stdint.h>

namespace timeprint {

class EdgeHook {
 public:
  explicit EdgeHook(bool activeHigh = false, uint32_t debounceMs = 30);
  bool update(bool rawLevel, uint32_t nowMs);

 private:
  bool activeHigh_;
  uint32_t debounceMs_;
  bool stableActive_;
  bool lastRaw_;
  uint32_t lastChangeMs_;
};

}  // namespace timeprint
