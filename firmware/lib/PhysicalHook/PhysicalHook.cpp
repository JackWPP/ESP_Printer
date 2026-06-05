#include "PhysicalHook.h"

namespace timeprint {

EdgeHook::EdgeHook(bool activeHigh, uint32_t debounceMs)
    : activeHigh_(activeHigh),
      debounceMs_(debounceMs),
      stableActive_(false),
      lastRaw_(false),
      lastChangeMs_(0) {}

bool EdgeHook::update(bool rawLevel, uint32_t nowMs) {
  bool active = activeHigh_ ? rawLevel : !rawLevel;

  if (active != lastRaw_) {
    lastRaw_ = active;
    lastChangeMs_ = nowMs;
    return false;
  }

  if ((nowMs - lastChangeMs_) >= debounceMs_ && stableActive_ != active) {
    stableActive_ = active;
    return stableActive_;
  }

  return false;
}

}  // namespace timeprint
