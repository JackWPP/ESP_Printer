#pragma once

#include "TimerCore.h"

namespace timeprint {

class Printer {
 public:
  virtual ~Printer() {}
  virtual void printSlip(const SlipData& slip) = 0;
  virtual void printSimple(const char* message) = 0;
};

}  // namespace timeprint
