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

  // 去抖前状态发生变化 -> 重置计时,不触发
  if (active != lastRaw_) {
    lastRaw_ = active;
    lastChangeMs_ = nowMs;
    return false;
  }

  // 状态已稳定超过去抖窗口,且与已确认状态不同 -> 提交
  if ((nowMs - lastChangeMs_) >= debounceMs_ && stableActive_ != active) {
    stableActive_ = active;
    return stableActive_;  // 仅在"进入 active 的稳定边沿"返回 true
  }
  return false;
}

}  // namespace timeprint
