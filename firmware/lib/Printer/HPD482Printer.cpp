#include "HPD482Printer.h"

#if defined(ARDUINO_ARCH_ESP32)

namespace timeprint {

HPD482Printer::HPD482Printer(HardwareSerial& serial, int rxPin, int txPin, uint32_t baud)
    : serial_(serial), rxPin_(rxPin), txPin_(txPin), baud_(baud) {}

bool HPD482Printer::begin(uint32_t readyTimeoutMs) {
  serial_.begin(baud_, SERIAL_8N1, rxPin_, txPin_);
  serial_.setTimeout(2000);
  Serial.printf("[HPD482] 等待 READY (超时 %lu ms)...\n", static_cast<unsigned long>(readyTimeoutMs));
  ready_ = waitReady(readyTimeoutMs);
  if (!ready_) {
    Serial.println(F("[HPD482] 未收到 READY，尝试发 AT"));
    serial_.print("AT");
    ready_ = waitOkSuffix("OK", 2000);
  }
  if (!ready_) {
    Serial.println(F("[HPD482] AT 也未回 OK，握手失败"));
    dumpSerialTail(F("[HPD482]"));
    return false;
  }

  // 配置：UTF-8 中文编码、24 号字体、回显 PT OK、打印次数 = 1。
  // 不再下发未在《AT 指令表 v4.6.5》内的 PM 之类未公开指令。
  bool configured = sendCommand("AT+EC=2") && sendCommand("AT+GU=1") &&
                    sendCommand("AT+CN=24") && sendCommand("AT+PC=1");
  ready_ = configured;
  if (!configured) {
    Serial.println(F("[HPD482] 配置指令失败"));
    dumpSerialTail(F("[HPD482]"));
    return ready_;
  }

  // 持久化上述配置，避免下次上电回到默认或异常态。
  sendCommand("AT+SV");
  Serial.println(F("[HPD482] 配置完成并保存"));
  return true;
}

bool HPD482Printer::sendCommand(const char* command, uint32_t timeoutMs) {
  if (!ready_ && strcmp(command, "AT") != 0) return false;
  flushInput();
  serial_.print(command);
  return waitOkSuffix("OK", timeoutMs);
}

bool HPD482Printer::printText(const char* text, uint32_t timeoutMs) {
  if (!ready_ || text == nullptr) return false;
  flushInput();
  writeEscapedText(text);
  return waitOkSuffix("PT OK", timeoutMs);
}

bool HPD482Printer::feedPaperSteps(int steps) {
  if (steps < 1) steps = 1;
  if (steps > 800) steps = 800;
  char command[20];
  snprintf(command, sizeof(command), "AT+GO=%d", steps);
  return sendCommand(command);
}

bool HPD482Printer::feedPaperMm(float mm) {
  return feedPaperSteps(static_cast<int>(mm * 8.0f + 0.5f));
}

String HPD482Printer::query(const char* command, uint32_t timeoutMs) {
  if (!ready_) return String();
  flushInput();
  serial_.print(command);
  String line;
  if (!waitLine(&line, timeoutMs)) return String();
  if (line.startsWith("Error")) return String();
  return line;
}

String HPD482Printer::version() {
  return query("AT+VS?");
}

float HPD482Printer::temperatureC() {
  String value = query("AT+TP?");
  if (value.length() == 0) return -999.0f;
  return value.toFloat();
}

bool HPD482Printer::resetPrinter() {
  if (!ready_) return false;
  flushInput();
  serial_.print("AT+RST");
  ready_ = waitReady(5000);
  return ready_;
}

void HPD482Printer::printSlip(const SlipData& slip) {
  char planned[8];
  char actual[8];
  char overrun[8];
  formatMMSS(slip.plannedSeconds, planned, sizeof(planned));
  formatMMSS(slip.actualSeconds, actual, sizeof(actual));
  formatMMSS(slip.overrunSeconds, overrun, sizeof(overrun));

  sendCommand("AT+CN=24");
  printText("------------------------");
  char line[48];
  snprintf(line, sizeof(line), "计划: %s", planned);
  printText(line);
  snprintf(line, sizeof(line), "实际: %s", actual);
  printText(line);
  snprintf(line, sizeof(line), "超时: %s", overrun);
  printText(line);
  printText("        :)");
  printText("------------------------");
  feedPaperMm(10);
}

void HPD482Printer::printSimple(const char* message) {
  Serial.println(F("[HPD482] printSimple 开始"));
  bool ok;

  ok = sendCommand("AT+CN=24");
  Serial.printf("[HPD482] AT+CN=24 → %d\n", ok ? 1 : 0);

  ok = printText("------------------------");
  Serial.printf("[HPD482] 分隔线 → %d\n", ok ? 1 : 0);

  ok = printText(message ? message : "物理计时器到时");
  Serial.printf("[HPD482] 消息 → %d\n", ok ? 1 : 0);

  char uptime[32];
  snprintf(uptime, sizeof(uptime), "运行: %lu秒", static_cast<unsigned long>(millis() / 1000));
  ok = printText(uptime);
  Serial.printf("[HPD482] 时间 → %d\n", ok ? 1 : 0);

  ok = printText("        :)");
  Serial.printf("[HPD482] 笑脸 → %d\n", ok ? 1 : 0);

  ok = printText("------------------------");
  Serial.printf("[HPD482] 分隔线 → %d\n", ok ? 1 : 0);

  ok = feedPaperMm(10);
  Serial.printf("[HPD482] 走纸 → %d\n", ok ? 1 : 0);

  Serial.println(F("[HPD482] printSimple 完成"));
}

void HPD482Printer::testPage() {
  if (!ready_) {
    Serial.println(F("[HPD482] testPage 在未就绪时调用，跳过"));
    return;
  }
  sendCommand("AT+CN=32");
  printText("=== TimePrint 打印机自检 ===");
  char buf[48];
  snprintf(buf, sizeof(buf), "固件版本: %lu", static_cast<unsigned long>(millis() / 1000));
  printText(buf);
  snprintf(buf, sizeof(buf), "波特率: %lu", static_cast<unsigned long>(baud_));
  printText(buf);
  printText("AT+EC=2 / AT+GU=1 / AT+CN=32");
  printText(":)");
  feedPaperMm(20);
}

bool HPD482Printer::waitReady(uint32_t timeoutMs) {
  const char* target = "READY";
  int match = 0;
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    while (serial_.available()) {
      char c = static_cast<char>(serial_.read());
      if (c == target[match]) {
        match++;
        if (match == 5) {
          uint32_t drainStart = millis();
          while (millis() - drainStart < 100) {
            while (serial_.available()) serial_.read();
          }
          return true;
        }
      } else {
        match = c == target[0] ? 1 : 0;
      }
    }
    delay(1);
  }
  return false;
}

bool HPD482Printer::waitLine(String* out, uint32_t timeoutMs) {
  String line;
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    while (serial_.available()) {
      char c = static_cast<char>(serial_.read());
      if (c == '\n') {
        line.trim();
        if (out) *out = line;
        return line.length() > 0;
      }
      if (c != '\r') line += c;
    }
    delay(1);
  }
  if (out) *out = line;
  return false;
}

bool HPD482Printer::waitOkSuffix(const char* suffix, uint32_t timeoutMs) {
  String line;
  if (!waitLine(&line, timeoutMs)) return false;
  return line.endsWith(suffix) && !line.startsWith("Error");
}

void HPD482Printer::flushInput() {
  while (serial_.available()) serial_.read();
}

void HPD482Printer::dumpSerialTail(const __FlashStringHelper* tag) {
  static const size_t kMaxDump = 96;
  char hex[kMaxDump * 3 + 1];
  char ascii[kMaxDump + 1];
  size_t hx = 0;
  size_t ax = 0;
  size_t count = 0;
  while (serial_.available() && count < kMaxDump) {
    int raw = serial_.read();
    if (raw < 0) break;
    unsigned char c = static_cast<unsigned char>(raw);
    if (hx + 3 < sizeof(hex)) {
      hx += snprintf(hex + hx, sizeof(hex) - hx, "%02X ", c);
    }
    ascii[ax++] = (c >= 0x20 && c < 0x7F) ? static_cast<char>(c) : '.';
    count++;
  }
  hex[hx] = '\0';
  ascii[ax] = '\0';
  Serial.print(tag);
  Serial.print(F(" 接收 "));
  Serial.print(count);
  Serial.print(F(" 字节 | hex: "));
  Serial.print(hex);
  Serial.print(F(" | ascii: "));
  Serial.println(ascii);
}

void HPD482Printer::writeEscapedText(const char* text) {
  if (text[0] == 'A' && text[1] == 'T') serial_.print('\\');
  serial_.print(text);
}

void HPD482Printer::formatMMSS(uint32_t seconds, char* out, size_t len) {
  snprintf(out, len, "%02u:%02u", static_cast<unsigned>(seconds / 60),
           static_cast<unsigned>(seconds % 60));
}

}  // namespace timeprint
#endif
