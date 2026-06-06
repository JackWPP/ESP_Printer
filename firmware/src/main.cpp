#include <Arduino.h>

#include "AlarmDetector.h"
#include "PhysicalHook.h"
#include "Printer.h"
#include "TimerCore.h"
#include "TimerCorePrinterBridge.h"

#if defined(ARDUINO_ARCH_ESP32)
#include "WebControlAdapter.h"
#include "WiFiManager.h"
#endif

using namespace timeprint;

#define ENABLE_PHYSICAL_HOOK 0
#define HOOK_ADC_PIN 4
#define ALARM_P2P_THRESHOLD 300
#define ALARM_WINDOW_MS 80
#define HOOK_DEBOUNCE_MS 500

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
};

static TimerCore core;
static SerialStatus serialStatus;
static PrinterStub printer;
static TimerCorePrinterBridge printerBridge(&printer);

#if defined(ARDUINO_ARCH_ESP32)
static TimePrintWiFiManager wifiManager;
static WebControlAdapter webControl(&core, &wifiManager);
#endif

static uint32_t lastTickMs = 0;
static uint32_t lastCalibMs = 0;
static String lineBuffer;

static void firePhysicalHook() {
  printer.printSimple("物理计时器到时");
}

static void printStatus() {
  Serial.printf("> 状态=%s 计划=%us 已用=%us 剩余=%us 超时=%us\n",
                stateToChinese(core.state()), static_cast<unsigned>(core.plannedSeconds()),
                static_cast<unsigned>(core.elapsedSeconds()),
                static_cast<unsigned>(core.remainingSeconds()),
                static_cast<unsigned>(core.overrunSeconds()));
}

static void printHelp() {
  Serial.println(F("命令: set <分钟> | start | pause | resume | stop | reset | hook | calib | status | wifiset <ssid> <密码> | wifireset | help"));
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
      Serial.println(F("> 用法: wifiset <ssid> <密码>"));
      return;
    }
    String ssid = command.substring(firstSpace + 1, secondSpace);
    String pass = command.substring(secondSpace + 1);
    Serial.printf("> 正在保存 %s 的 WiFi 凭据并重启\n", ssid.c_str());
    wifiManager.saveCredentials(ssid, pass);
#else
    Serial.println(F("> WiFi 配网只在 ESP32 构建中可用"));
#endif
  } else if (command == "wifireset") {
#if defined(ARDUINO_ARCH_ESP32)
    Serial.println(F("> 正在清除 WiFi 凭据并重启"));
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
    if (physicalHook.update(oscillating, millis())) firePhysicalHook();
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
