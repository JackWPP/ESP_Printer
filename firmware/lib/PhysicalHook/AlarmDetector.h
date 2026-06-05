#pragma once

#include <stdint.h>

namespace timeprint {

class AlarmDetector {
 public:
  AlarmDetector(int p2pThreshold, uint32_t windowMs);

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
