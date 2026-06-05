#pragma once
#include "Printer.h"
#include "TimerCore.h"

namespace timeprint {

// 把 TimerCore 的 onPrintSlip 效果转交给 Printer 设备。
// 好处:TimerCore 不需要知道打印机存在,Printer 也不依赖 TimerCore 的监听机制,
// 两者通过这个 bridge 解耦。
class TimerCorePrinterBridge : public ITimerCoreListener {
 public:
  explicit TimerCorePrinterBridge(Printer* printer) : printer_(printer) {}

  void onPrintSlip(const SlipData& slip) override {
    if (printer_) printer_->printSlip(slip);
  }

 private:
  Printer* printer_;
};

}  // namespace timeprint
