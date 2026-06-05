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

class SerialStatus : public ITimerCoreListener {
 public:
  void onStateChanged(State, State to) override {
    Serial.print(F("[STATE] "));
    Serial.println(stateToString(to));
  }

  void onTick(const TickInfo& info) override {
    char remaining[8];
    char overrun[8];
    formatMMSS(info.remainingSeconds, remaining, sizeof(remaining));
    formatMMSS(info.overrunSeconds, overrun, sizeof(overrun));
    Serial.printf("[TICK] planned=%us elapsed=%us remaining=%s overrun=%s\n",
                  static_cast<unsigned>(info.plannedSeconds),
                  static_cast<unsigned>(info.elapsedSeconds), remaining, overrun);
  }

  void onFeedback(Feedback fb) override {
    const char* value = fb == Feedback::Start    ? "START"
                        : fb == Feedback::TimeUp ? "TIME-UP"
                        : fb == Feedback::Stop   ? "STOP"
                                                 : "RESET";
    Serial.print(F("[FEEDBACK] "));
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

    Serial.println(F("\n----- [PRINT] slip: software path -----"));
    Serial.printf("  planned : %s\n", planned);
    Serial.printf("  actual  : %s\n", actual);
    Serial.printf("  overrun : %s\n", overrun);
    Serial.println(F("            :)"));
    Serial.println(F("---------------------------------------\n"));
  }

  void printSimple(const char* message) override {
    Serial.println(F("\n----- [PRINT] slip: physical hook -----"));
    Serial.printf("  %s\n", message);
    Serial.printf("  uptime: %lus\n", static_cast<unsigned long>(millis() / 1000));
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
  printer.printSimple("Physical timer elapsed");
}

static void printStatus() {
  Serial.printf("> state=%s planned=%us elapsed=%us remaining=%us overrun=%us\n",
                stateToString(core.state()), static_cast<unsigned>(core.plannedSeconds()),
                static_cast<unsigned>(core.elapsedSeconds()),
                static_cast<unsigned>(core.remainingSeconds()),
                static_cast<unsigned>(core.overrunSeconds()));
}

static void printHelp() {
  Serial.println(F("commands: set <min> | start | pause | resume | stop | reset | hook | calib | status | wifiset <ssid> <pass> | wifireset | help"));
}

static void handleSerialCommand(String command) {
  command.trim();
  if (command.length() == 0) return;

  if (command.startsWith("set")) {
    int separator = command.indexOf(' ');
    uint32_t minutes = separator > 0 ? static_cast<uint32_t>(command.substring(separator + 1).toInt()) : 0;
    core.setTime(minutes);
    Serial.printf("> set %u min\n", static_cast<unsigned>(minutes));
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
    Serial.printf("> calib %s (GPIO%d)\n", calibOn ? "ON" : "OFF", HOOK_ADC_PIN);
  } else if (command == "status") {
    printStatus();
  } else if (command.startsWith("wifiset ")) {
#if defined(ARDUINO_ARCH_ESP32)
    int firstSpace = command.indexOf(' ');
    int secondSpace = command.indexOf(' ', firstSpace + 1);
    if (firstSpace < 0 || secondSpace < 0 || secondSpace == command.length() - 1) {
      Serial.println(F("> usage: wifiset <ssid> <pass>"));
      return;
    }
    String ssid = command.substring(firstSpace + 1, secondSpace);
    String pass = command.substring(secondSpace + 1);
    Serial.printf("> saving WiFi credentials for %s and restarting\n", ssid.c_str());
    wifiManager.saveCredentials(ssid, pass);
#else
    Serial.println(F("> WiFi setup is only available on ESP32 builds"));
#endif
  } else if (command == "wifireset") {
#if defined(ARDUINO_ARCH_ESP32)
    Serial.println(F("> clearing WiFi credentials and restarting"));
    wifiManager.resetWiFi();
#else
    Serial.println(F("> WiFi reset is only available on ESP32 builds"));
#endif
  } else if (command == "help") {
    printHelp();
  } else {
    Serial.print(F("? unknown command: "));
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

  Serial.println(F("\n=== TimePrint firmware scaffold ==="));
  printHelp();

#if defined(ARDUINO_ARCH_ESP32)
  wifiManager.begin();
  webControl.begin();
  Serial.printf("[Web] open http://%s/\n", wifiManager.ipString().c_str());
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
      Serial.printf("[CALIB] adc=%4d p2p=%4d osc=%d threshold=%d\n", sample,
                    alarmDetector.lastP2P(), oscillating ? 1 : 0, ALARM_P2P_THRESHOLD);
    }
  }

  uint32_t now = millis();
  if (now - lastTickMs >= 1000) {
    lastTickMs = now;
    core.tick();
  }
}
