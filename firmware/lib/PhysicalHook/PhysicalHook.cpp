#include "PhysicalHook.h"

namespace timeprint {

EdgeHook::EdgeHook(bool activeHigh, uint32_t silenceMs)
    : activeHigh_(activeHigh),
      silenceMs_(silenceMs),
      state_(State::Armed),
      silenceStartMs_(0) {}

bool EdgeHook::update(bool rawLevel, uint32_t nowMs) {
  bool active = activeHigh_ ? rawLevel : !rawLevel;

  switch (state_) {
    case State::Armed:
      if (active) {
        state_ = State::Alarmed;
        return true;  // 触发！
      }
      return false;

    case State::Alarmed:
      if (!active) {
        // 报警停止（人关了计时器），开始计静默
        state_ = State::WaitSilence;
        silenceStartMs_ = nowMs;
      }
      // active=true 或 刚转 false：都不触发
      return false;

    case State::WaitSilence:
      if (active) {
        // 报警又来了（人重新开了？），回到 Alarmed
        state_ = State::Alarmed;
        return false;
      }
      // 持续静默
      if ((nowMs - silenceStartMs_) >= silenceMs_) {
        state_ = State::Armed;
      }
      return false;
  }

  return false;
}

}  // namespace timeprint
