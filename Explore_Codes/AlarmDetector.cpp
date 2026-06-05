#include "AlarmDetector.h"

namespace timeprint {

AlarmDetector::AlarmDetector(int p2pThreshold, uint32_t windowMs)
    : p2pThreshold_(p2pThreshold),
      windowMs_(windowMs),
      winMin_(0),
      winMax_(0),
      haveSample_(false),
      winStartMs_(0),
      oscillating_(false),
      lastP2P_(0) {}

bool AlarmDetector::update(int sample, uint32_t nowMs) {
  if (!haveSample_) {
    winMin_ = winMax_ = sample;
    winStartMs_ = nowMs;
    haveSample_ = true;
  } else {
    if (sample < winMin_) winMin_ = sample;
    if (sample > winMax_) winMax_ = sample;
  }

  // 窗口到时:结算峰峰值,刷新判定,开新窗口
  if ((nowMs - winStartMs_) >= windowMs_) {
    lastP2P_ = winMax_ - winMin_;
    oscillating_ = (lastP2P_ >= p2pThreshold_);
    winMin_ = winMax_ = sample;
    winStartMs_ = nowMs;
  }
  return oscillating_;
}

}  // namespace timeprint
