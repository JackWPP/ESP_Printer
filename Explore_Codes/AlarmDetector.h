#pragma once
#include <stdint.h>

namespace timeprint {

// 振荡检测器:喂入连续采样(如 ADC counts),在一个时间窗口内统计峰峰值;
// 峰峰值 >= 阈值 => "正在振荡"(报警)。
// 只看峰峰、不看绝对直流电平 => 静音档(1.3~1.8V)/蜂鸣档(2.9~3.2V)两种模式通吃。
class AlarmDetector {
 public:
  // p2pThreshold: 判为"在振荡"的峰峰阈值(单位同喂入的采样,通常是 ADC counts)
  // windowMs: 统计窗口长度
  AlarmDetector(int p2pThreshold, uint32_t windowMs);

  // 喂一个采样;返回当前是否判定为"在振荡"
  bool update(int sample, uint32_t nowMs);

  bool isOscillating() const { return oscillating_; }
  int lastP2P() const { return lastP2P_; }

 private:
  int p2pThreshold_;
  uint32_t windowMs_;
  int winMin_;
  int winMax_;
  bool haveSample_;
  uint32_t winStartMs_;
  bool oscillating_;
  int lastP2P_;
};

}  // namespace timeprint
