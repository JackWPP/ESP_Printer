#pragma once

#include "Printer.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <Arduino.h>

namespace timeprint {

class HPD482Printer : public Printer {
 public:
  HPD482Printer(HardwareSerial& serial, int rxPin, int txPin, uint32_t baud = 115200);

  bool begin(uint32_t readyTimeoutMs = 5000);
  bool ready() const { return ready_; }

  bool sendCommand(const char* command, uint32_t timeoutMs = 3000);
  bool printText(const char* text, uint32_t timeoutMs = 5000);
  bool feedPaperSteps(int steps);
  bool feedPaperMm(float mm);
  String query(const char* command, uint32_t timeoutMs = 3000);
  String version();
  float temperatureC();
  bool resetPrinter();

  void printSlip(const SlipData& slip) override;
  void printSimple(const char* message) override;

 private:
  HardwareSerial& serial_;
  int rxPin_;
  int txPin_;
  uint32_t baud_;
  bool ready_ = false;

  bool waitReady(uint32_t timeoutMs);
  bool waitLine(String* out, uint32_t timeoutMs);
  bool waitOkSuffix(const char* suffix, uint32_t timeoutMs);
  void flushInput();
  void writeEscapedText(const char* text);
  static void formatMMSS(uint32_t seconds, char* out, size_t len);
};

}  // namespace timeprint
#endif
