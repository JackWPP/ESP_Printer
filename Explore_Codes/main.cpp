#include <Arduino.h>
#include "TimerCore.h"
#include "Printer.h"
#include "TimerCorePrinterBridge.h"
#include "PhysicalHook.h"
#include "AlarmDetector.h"

using namespace timeprint;

// ============================================================================
// 两条打印触发路径,汇到同一个 Printer:
//   A. 软件路径: TimerCore(web/app 控制) --onPrintSlip--> bridge --> Printer.printSlip
//   B. 硬件路径: 机芯蓝色信号线(报警时振荡) --ADC--> AlarmDetector --> EdgeHook
//                 --> Printer.printSimple
//
// 接法: 蓝色信号线 --> ÷2 分压(10k/10k) --> ADC1 脚(GPIO1~10);与电池负极共地。
//        判据是"在不在振荡",与静音/蜂鸣档无关 => 静音也能触发打印。
// ============================================================================

// ---- 物理 hook 配置(接好线、用 calib 校准阈值后再开启)----
#define ENABLE_PHYSICAL_HOOK 0      // 接好线并校准后改成 1
#define HOOK_ADC_PIN 4              // TODO: 必须是 ADC1 脚(GPIO1~10);ADC2 在 WiFi 下不可用
#define ALARM_P2P_THRESHOLD 300     // 峰峰阈值(ADC counts),用 calib 实测后调整
#define ALARM_WINDOW_MS 80          // 振荡统计窗口
#define HOOK_DEBOUNCE_MS 500        // 桥接蜂鸣"嘀嘀"间隙,使整段报警只触发一次

static AlarmDetector alarmDet(ALARM_P2P_THRESHOLD, ALARM_WINDOW_MS);
static EdgeHook hook(/*activeHigh=*/true, /*debounceMs=*/HOOK_DEBOUNCE_MS);  // 振荡=active
static bool calibOn = false;        // 串口 'calib' 切换:实时打印 ADC/峰峰,用于定阈值

static const char* stateName(State s) {
  switch (s) {
    case State::Idle:     return "IDLE";
    case State::Running:  return "RUNNING";
    case State::Overtime: return "OVERTIME";
    case State::Paused:   return "PAUSED";
    case State::Finished: return "FINISHED";
  }
  return "?";
}

static void fmtMMSS(uint32_t sec, char* out, size_t n) {
  snprintf(out, n, "%02u:%02u", (unsigned)(sec / 60), (unsigned)(sec % 60));
}

// 状态输出 adapter(将来由 WebSocket / BLE notify 替代或并存)
class SerialStatus : public ITimerCoreListener {
 public:
  void onStateChanged(State, State to) override {
    Serial.print(F("[STATE] "));
    Serial.println(stateName(to));
  }
  void onTick(const TickInfo& i) override {
    char rem[8], ovr[8];
    fmtMMSS(i.remainingSeconds, rem, sizeof(rem));
    fmtMMSS(i.overrunSeconds, ovr, sizeof(ovr));
    Serial.printf("[TICK] planned=%us elapsed=%us remaining=%s overrun=%s\n",
                  (unsigned)i.plannedSeconds, (unsigned)i.elapsedSeconds, rem, ovr);
  }
  void onFeedback(Feedback fb) override {
    const char* f = (fb == Feedback::Start)  ? "START"
                  : (fb == Feedback::TimeUp) ? "TIME-UP (red->white)"
                  : (fb == Feedback::Stop)   ? "STOP"
                                             : "RESET";
    Serial.print(F("[FEEDBACK] "));
    Serial.println(f);
  }
};

// 打印机设备。P0 = 串口降级实现。打印机到货后新建 ThermalPrinter : Printer 替换。
class PrinterStub : public Printer {
 public:
  void printSlip(const SlipData& s) override {
    char planned[8], actual[8], over[8];
    fmtMMSS(s.plannedSeconds, planned, sizeof(planned));
    fmtMMSS(s.actualSeconds, actual, sizeof(actual));
    fmtMMSS(s.overrunSeconds, over, sizeof(over));
    Serial.println(F("\n----- [PRINT] 便签(软件路径)-----"));
    Serial.printf("  预计 planned : %s\n", planned);
    Serial.printf("  实际 actual  : %s\n", actual);
    Serial.printf("  超时 overrun : %s\n", over);
    Serial.println(F("            :)            "));
    Serial.println(F("-----------------------------------\n"));
  }
  void printSimple(const char* message) override {
    Serial.println(F("\n----- [PRINT] 便签(硬件 hook)-----"));
    Serial.printf("  %s\n", message);
    Serial.printf("  uptime: %lus\n", (unsigned long)(millis() / 1000));
    Serial.println(F("            :)            "));
    Serial.println(F("-----------------------------------\n"));
  }
};

TimerCore core;
SerialStatus statusOut;
PrinterStub printer;
TimerCorePrinterBridge printBridge(&printer);  // 软件路径 -> 打印机

uint32_t lastTickMs = 0;
uint32_t lastCalibMs = 0;
String lineBuf;

void firePhysicalHook() {
  printer.printSimple("物理计时器到时");
}

void handleCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  if (cmd.startsWith("set")) {
    int sp = cmd.indexOf(' ');
    uint32_t minutes = (sp > 0) ? (uint32_t)cmd.substring(sp + 1).toInt() : 0;
    core.setTime(minutes);
    Serial.printf("> set %u min\n", (unsigned)minutes);
  } else if (cmd == "start") {
    core.start();
  } else if (cmd == "pause") {
    core.pause();
  } else if (cmd == "resume") {
    core.resume();
  } else if (cmd == "stop") {
    core.stop();
  } else if (cmd == "reset") {
    core.reset();
  } else if (cmd == "hook") {
    firePhysicalHook();  // 没接硬件时模拟机芯触发,验证 hook 打印路径
  } else if (cmd == "calib") {
    calibOn = !calibOn;  // 切换校准模式:实时打印 ADC/峰峰,用来定 ALARM_P2P_THRESHOLD
    Serial.printf("> calib %s (读 GPIO%d)\n", calibOn ? "ON" : "OFF", HOOK_ADC_PIN);
  } else if (cmd == "status") {
    Serial.printf("> state=%s planned=%us elapsed=%us\n",
                  stateName(core.state()),
                  (unsigned)core.plannedSeconds(),
                  (unsigned)core.elapsedSeconds());
  } else if (cmd == "help") {
    Serial.println(F("commands: set <min> | start | pause | resume | stop | reset | hook | calib | status"));
  } else {
    Serial.print(F("? unknown: "));
    Serial.print(cmd);
    Serial.println(F("  (try 'help')"));
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  core.addListener(&statusOut);
  core.addListener(&printBridge);
  Serial.println(F("\n=== TimePrint P0 (serial harness) ==="));
  Serial.println(F("type 'help'.  set 1 / start / stop | hook 模拟触发 | calib 校准报警阈值"));
}

void loop() {
  // 串口命令输入
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (lineBuf.length() > 0) {
        handleCommand(lineBuf);
        lineBuf = "";
      }
    } else {
      lineBuf += c;
    }
  }

  // ADC 采样:hook 启用时或校准时都需要采
  bool sampling = calibOn;
#if ENABLE_PHYSICAL_HOOK
  sampling = true;
#endif
  if (sampling) {
    int s = analogRead(HOOK_ADC_PIN);
    bool osc = alarmDet.update(s, millis());
#if ENABLE_PHYSICAL_HOOK
    if (hook.update(osc, millis())) {
      firePhysicalHook();  // 硬件 hook 路径:机芯报警 -> 打印
    }
#endif
    if (calibOn && (millis() - lastCalibMs >= 200)) {
      lastCalibMs = millis();
      Serial.printf("[CALIB] adc=%4d  p2p=%4d  osc=%d  (阈值=%d)\n",
                    s, alarmDet.lastP2P(), osc ? 1 : 0, ALARM_P2P_THRESHOLD);
    }
  }

  // 1Hz tick(软件计时)
  uint32_t now = millis();
  if (now - lastTickMs >= 1000) {
    lastTickMs = now;
    core.tick();
  }
}
