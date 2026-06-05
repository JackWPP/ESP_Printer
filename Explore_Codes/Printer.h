#pragma once
#include "TimerCore.h"  // for SlipData

namespace timeprint {

// 打印机"设备"抽象。
// 软件计时路径(TimerCore)与硬件 hook 路径都通过它打印,彼此不耦合。
// P0 用串口降级实现 PrinterStub;打印机到货后换成 ThermalPrinter,实现同接口即可。
class Printer {
 public:
  virtual ~Printer() {}

  // 软件路径:富便签(预计 / 实际 / 超时)
  virtual void printSlip(const SlipData& slip) = 0;

  // 硬件 hook 路径:简单便签(机芯到时,读不到 planned/actual)
  virtual void printSimple(const char* message) = 0;
};

}  // namespace timeprint
