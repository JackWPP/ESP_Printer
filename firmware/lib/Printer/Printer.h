#pragma once

#include "TimerCore.h"

namespace timeprint {

class Printer {
 public:
  virtual ~Printer() {}
  virtual void printSlip(const SlipData& slip) = 0;
  virtual void printSimple(const char* message) = 0;
  // 设备标识，给后台 / 调试用
  virtual const char* name() const = 0;
  // 自检页
  virtual void testPage() = 0;
};

}  // namespace timeprint
