#include <Arduino.h>

#include "AlarmDetector.h"
#include "PhysicalHook.h"
#include "Printer.h"
#include "PrinterConfig.h"
#include "TimerCore.h"
#include "TimerCorePrinterBridge.h"

#if defined(ARDUINO_ARCH_ESP32)
#include "HPD482Printer.h"
#include "WebControlAdapter.h"
#include "WiFiManager.h"
#endif

using namespace timeprint;

#define ENABLE_PHYSICAL_HOOK 1
#define HOOK_ADC_PIN 4
#define ALARM_P2P_THRESHOLD 300
#define ALARM_WINDOW_MS 80
#define HOOK_DEBOUNCE_MS 3000

static AlarmDetector alarmDetector(ALARM_P2P_THRESHOLD, ALARM_WINDOW_MS);
static EdgeHook physicalHook(true, HOOK_DEBOUNCE_MS);
static bool calibOn = false;

static void formatMMSS(uint32_t seconds, char* out, size_t len) {
  snprintf(out, len, "%02u:%02u", static_cast<unsigned>(seconds / 60),
           static_cast<unsigned>(seconds % 60));
}

static const char* stateToChinese(State state) {
  switch (state) {
    case State::Idle:
      return "待机";
    case State::Running:
      return "计时中";
    case State::Paused:
      return "已暂停";
    case State::Overtime:
      return "已超时";
  }
  return "未知";
}

class SerialStatus : public ITimerCoreListener {
 public:
  void onStateChanged(State, State to) override {
    Serial.print(F("[状态] "));
    Serial.println(stateToChinese(to));
  }

  void onTick(const TickInfo& info) override {
    char remaining[8];
    char overrun[8];
    formatMMSS(info.remainingSeconds, remaining, sizeof(remaining));
    formatMMSS(info.overrunSeconds, overrun, sizeof(overrun));
    Serial.printf("[计时] 计划=%us 已用=%us 剩余=%s 超时=%s\n",
                  static_cast<unsigned>(info.plannedSeconds),
                  static_cast<unsigned>(info.elapsedSeconds), remaining, overrun);
  }

  void onFeedback(Feedback fb) override {
    const char* value = fb == Feedback::Start    ? "开始"
                        : fb == Feedback::TimeUp ? "到时"
                        : fb == Feedback::Stop   ? "停止"
                                                 : "重置";
    Serial.print(F("[反馈] "));
    Serial.println(value);
  }
};

class PrinterStub : public Printer {
 public:
  void printSlip(const SlipData& slip) override {
    char planned[8];
    char actual[8];
    char overrun[8];
    formatMMSS(slip.plannedSeconds, planned, sizeof(planned));
    formatMMSS(slip.actualSeconds, actual, sizeof(actual));
    formatMMSS(slip.overrunSeconds, overrun, sizeof(overrun));

    Serial.println(F("\n----- [打印] 便签：软件计时路径 -----"));
    Serial.printf("  计划时间：%s\n", planned);
    Serial.printf("  实际用时：%s\n", actual);
    Serial.printf("  超时时间：%s\n", overrun);
    Serial.println(F("            :)"));
    Serial.println(F("---------------------------------------\n"));
  }

  void printSimple(const char* message) override {
    Serial.println(F("\n----- [打印] 便签：物理 hook 路径 -----"));
    Serial.printf("  %s\n", message);
    Serial.printf("  运行时间：%lu 秒\n", static_cast<unsigned long>(millis() / 1000));
    Serial.println(F("            :)"));
    Serial.println(F("---------------------------------------\n"));
  }

  const char* name() const override { return "stub"; }
  void testPage() override {
    Serial.println(F("\n----- [打印] 自检：PrinterStub -----"));
    Serial.println(F("  当前没有真实打印机，仅串口日志"));
    Serial.println(F("  TimePrint stub printer v1"));
    Serial.println(F("---------------------------------------\n"));
  }
};

static TimerCore core;
static SerialStatus serialStatus;
static PrinterStub stubPrinter;
#if defined(ARDUINO_ARCH_ESP32)
static HPD482Printer hpdPrinter(Serial2, PRINTER_RX_PIN, PRINTER_TX_PIN);
#endif
static Printer* activePrinter = &stubPrinter;
static TimerCorePrinterBridge printerBridge(activePrinter);

#if defined(ARDUINO_ARCH_ESP32)
static TimePrintWiFiManager wifiManager;
static WebControlAdapter webControl(&core, &wifiManager);
#endif

static uint32_t lastTickMs = 0;
static uint32_t lastCalibMs = 0;
static String lineBuffer;

static void firePhysicalHook() {
  if (activePrinter) activePrinter->printSimple("物理计时器到时");
}

static void printStatus() {
  Serial.printf("> 状态=%s 计划=%us 已用=%us 剩余=%us 超时=%us\n",
                stateToChinese(core.state()), static_cast<unsigned>(core.plannedSeconds()),
                static_cast<unsigned>(core.elapsedSeconds()),
                static_cast<unsigned>(core.remainingSeconds()),
                static_cast<unsigned>(core.overrunSeconds()));
}

static void printHelp() {
  Serial.println(F("命令: set <分钟> | start | pause | resume | stop | reset | hook | calib | status | wifilist [list|add|del|reset] | wifiset <ssid> <密码> | wifireset | help"));
}

static void handleSerialCommand(String command) {
  command.trim();
  if (command.length() == 0) return;

  if (command.startsWith("set")) {
    int separator = command.indexOf(' ');
    uint32_t minutes = separator > 0 ? static_cast<uint32_t>(command.substring(separator + 1).toInt()) : 0;
    core.setTime(minutes);
    Serial.printf("> 已设置 %u 分钟\n", static_cast<unsigned>(minutes));
  } else if (command == "start") {
    core.start();
  } else if (command == "pause") {
    core.pause();
  } else if (command == "resume") {
    core.resume();
  } else if (command == "stop") {
    core.stop();
  } else if (command == "reset") {
    core.reset();
  } else if (command == "hook") {
    firePhysicalHook();
  } else if (command == "calib") {
    calibOn = !calibOn;
    Serial.printf("> 校准 %s (GPIO%d)\n", calibOn ? "开启" : "关闭", HOOK_ADC_PIN);
  } else if (command == "status") {
    printStatus();
  } else if (command.startsWith("wifiset ")) {
#if defined(ARDUINO_ARCH_ESP32)
    int firstSpace = command.indexOf(' ');
    int secondSpace = command.indexOf(' ', firstSpace + 1);
    if (firstSpace < 0 || secondSpace < 0 || secondSpace == command.length() - 1) {
      Serial.println(F("> 用法: wifiset <ssid> <密码>  (等价 wifilist add)"));
      return;
    }
    String ssid = command.substring(firstSpace + 1, secondSpace);
    String pass = command.substring(secondSpace + 1);
    Serial.printf("> 正在保存 %s 的 WiFi 凭据并重启\n", ssid.c_str());
    wifiManager.saveCredentials(ssid, pass);
#else
    Serial.println(F("> WiFi 配网只在 ESP32 构建中可用"));
#endif
  } else if (command == "wifilist" || command.startsWith("wifilist ")) {
#if defined(ARDUINO_ARCH_ESP32)
    String sub = (command.length() == 8) ? String("list") : command.substring(9);
    sub.trim();
    int sp = sub.indexOf(' ');
    String verb = (sp < 0) ? sub : sub.substring(0, sp);
    String rest = (sp < 0) ? String() : sub.substring(sp + 1);

    if (verb == "list" || verb.length() == 0) {
      int n = wifiManager.credentialCount();
      Serial.printf("> WiFi 凭据 (%d/%d)%s\n", n, TimePrintWiFiManager::kMaxCredentials,
                    wifiManager.hasDefaultCredential() ? " [NVS 为空，使用出厂默认]" : "");
      for (int i = 0; i < n; ++i) {
        Serial.printf("  [%d] %s\n", i, wifiManager.credentialSsid(i).c_str());
      }
    } else if (verb == "add") {
      int sp2 = rest.indexOf(' ');
      if (sp2 < 0) {
        Serial.println(F("> 用法: wifilist add <ssid> <密码>"));
        return;
      }
      String ssid = rest.substring(0, sp2);
      String pass = rest.substring(sp2 + 1);
      if (ssid.length() == 0) {
        Serial.println(F("> SSID 不能为空"));
        return;
      }
      Serial.printf("> 正在添加 %s 并重启\n", ssid.c_str());
      wifiManager.saveCredentials(ssid, pass);
    } else if (verb == "del") {
      int idx = rest.toInt();
      if (idx < 0 || idx >= wifiManager.credentialCount()) {
        Serial.printf("> 索引无效 (0..%d)\n", wifiManager.credentialCount() - 1);
        return;
      }
      String ssid = wifiManager.credentialSsid(idx);
      wifiManager.removeCredential(idx);
      Serial.printf("> 已删除 [%d] %s，重启中\n", idx, ssid.c_str());
      wifiManager.commitAndRestart();
    } else if (verb == "reset") {
      Serial.println(F("> 正在清除全部 WiFi 凭据并重启"));
      wifiManager.resetWiFi();
    } else {
      Serial.println(F("> 用法: wifilist [list|add <ssid> <pass>|del <idx>|reset]"));
    }
#else
    Serial.println(F("> WiFi 列表管理只在 ESP32 构建中可用"));
#endif
  } else if (command == "wifireset") {
#if defined(ARDUINO_ARCH_ESP32)
    Serial.println(F("> 正在清除全部 WiFi 凭据并重启"));
    wifiManager.resetWiFi();
#else
    Serial.println(F("> WiFi 重置只在 ESP32 构建中可用"));
#endif
  } else if (command == "help") {
    printHelp();
  } else {
    Serial.print(F("? 未知命令: "));
    Serial.println(command);
    printHelp();
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);

  core.addListener(&serialStatus);
  core.addListener(&printerBridge);
#if defined(ARDUINO_ARCH_ESP32)
  core.addListener(&webControl);
#endif

  Serial.println(F("\n=== TimePrint 固件骨架 ==="));
  printHelp();

#if defined(ARDUINO_ARCH_ESP32)
  Serial.printf("[打印机] 正在尝试 HPD482 (Serial2 RX=%d TX=%d)\n", PRINTER_RX_PIN, PRINTER_TX_PIN);
  if (hpdPrinter.begin(PRINTER_READY_TIMEOUT_MS)) {
    Serial.println(F("[打印机] HPD482 就绪，使用真实打印机"));
    printerBridge.setPrinter(&hpdPrinter);
    webControl.setActivePrinter(&hpdPrinter);
  } else {
    Serial.println(F("[打印机] HPD482 未就绪，降级到 PrinterStub 串口日志"));
  }
#endif

#if defined(ARDUINO_ARCH_ESP32)
  wifiManager.begin();
  webControl.begin();
  Serial.printf("[Web] 打开 http://%s/\n", wifiManager.ipString().c_str());
#endif
}

void loop() {
#if defined(ARDUINO_ARCH_ESP32)
  wifiManager.loop();
  webControl.loop();
#endif

  while (Serial.available()) {
    char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      if (lineBuffer.length() > 0) {
        handleSerialCommand(lineBuffer);
        lineBuffer = "";
      }
    } else {
      lineBuffer += c;
    }
  }

  bool sampling = calibOn;
#if ENABLE_PHYSICAL_HOOK
  sampling = true;
#endif

  if (sampling) {
    int sample = analogRead(HOOK_ADC_PIN);
    bool oscillating = alarmDetector.update(sample, millis());
#if ENABLE_PHYSICAL_HOOK
    bool hookFired = physicalHook.update(oscillating, millis());
    if (hookFired) {
      Serial.println(F("[hook] 物理 hook 触发！正在打印..."));
      firePhysicalHook();
      Serial.println(F("[hook] 打印完成"));
    }
#endif
    if (calibOn && millis() - lastCalibMs >= 200) {
      lastCalibMs = millis();
      Serial.printf("[校准] adc=%4d p2p=%4d 振荡=%d 阈值=%d\n", sample,
                    alarmDetector.lastP2P(), oscillating ? 1 : 0, ALARM_P2P_THRESHOLD);
    }
  }

  uint32_t now = millis();
  if (now - lastTickMs >= 1000) {
    lastTickMs = now;
    core.tick();
  }
}
