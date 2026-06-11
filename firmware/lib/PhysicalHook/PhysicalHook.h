#pragma once

#include <stdint.h>

namespace timeprint {

// 物理报警单次触发钩子。
//
// 状态机：
//   Armed   → active=true → 触发，进入 Alarmed
//   Alarmed → active 持续 → 保持（不重复触发）
//   Alarmed → active 消失 → 进入 WaitSilence
//   WaitSilence → active 持续 false 达到 silenceMs → 回到 Armed
//   WaitSilence → active 又变 true → 回到 Alarmed（同一段报警）
//
// 效果：一段报警只出一张纸，报警完全停止后等 silenceMs 才重新就绪。
class EdgeHook {
 public:
  explicit EdgeHook(bool activeHigh = false, uint32_t silenceMs = 3000);
  bool update(bool rawLevel, uint32_t nowMs);

 private:
  enum class State : uint8_t { Armed, Alarmed, WaitSilence };

  bool activeHigh_;
  uint32_t silenceMs_;
  State state_;
  uint32_t silenceStartMs_;  // 进入 WaitSilence 的时刻
};

}  // namespace timeprint
