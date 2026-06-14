#pragma once

#include "Printer.h"
#include "TimerCore.h"

namespace timeprint {

class TimerCorePrinterBridge : public ITimerCoreListener {
 public:
  explicit TimerCorePrinterBridge(Printer* printer) : printer_(printer) {}

  void setPrinter(Printer* printer) { printer_ = printer; }
  Printer* printer() const { return printer_; }

  void setTemplateMessage(const char* msg) { templateMessage_ = msg; }
  const char* templateMessage() const { return templateMessage_; }

  void onPrintSlip(const SlipData& slip) override {
    if (!printer_) return;
    if (templateMessage_) {
      printer_->printSlip(slip, templateMessage_);
    } else {
      printer_->printSlip(slip);
    }
  }

 private:
  Printer* printer_;
  const char* templateMessage_ = nullptr;
};

}  // namespace timeprint
