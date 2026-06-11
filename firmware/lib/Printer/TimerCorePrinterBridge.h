#pragma once

#include "Printer.h"
#include "TimerCore.h"

namespace timeprint {

class TimerCorePrinterBridge : public ITimerCoreListener {
 public:
  explicit TimerCorePrinterBridge(Printer* printer) : printer_(printer) {}

  void setPrinter(Printer* printer) { printer_ = printer; }
  Printer* printer() const { return printer_; }

  void onPrintSlip(const SlipData& slip) override {
    if (printer_) printer_->printSlip(slip);
  }

 private:
  Printer* printer_;
};

}  // namespace timeprint
