#pragma once
#include <stdint.h>

namespace timeprint {

// 可移植的去抖边沿检测器,用于机芯"到时"hook。
// 设计成不碰硬件:main 每轮把"原始电平 + 当前 ms"喂进来,
// 在检测到一次有效触发(进入 active 的稳定边沿)时返回 true。
// 引脚号、有效极性、是否加光耦,全部由 main 决定 —— 等实测确认后再填。
class EdgeHook {
 public:
  // activeHigh: 触发是否高有效(默认 false = 低有效,配合内部上拉/接通到地)
  // debounceMs: 去抖窗口
  explicit EdgeHook(bool activeHigh = false, uint32_t debounceMs = 30);

  // 喂入一次采样;返回 true 表示"本次检测到一次有效触发"
  bool update(bool rawLevel, uint32_t nowMs);

 private:
  bool activeHigh_;
  uint32_t debounceMs_;
  bool stableActive_;   // 已确认的稳定状态
  bool lastRaw_;        // 最近一次的去抖前状态
  uint32_t lastChangeMs_;
};

}  // namespace timeprint
