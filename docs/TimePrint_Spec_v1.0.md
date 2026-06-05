# TimePrint — 主开发规格文档 v1.0

> 时光小印 · 北京化工大学 产品设计 2303
> 本文档整合了全部设计决策、硬件实测数据与软件架构。
> 供 Claude Code agent 及后续开发直接使用,无需参考历史对话。

---

## 0. 快速入口

| 项 | 内容 |
|---|---|
| 当前状态 | **P0 已完成**(见 §8.1) |
| 下一任务 | **P1 Web/WiFi**(见 §8.2) |
| 代码库 | `firmware/`(PlatformIO),`app-flutter/`,`server/` |
| 铁律 | **永远不要修改 `lib/TimerCore`**,它已通过完整单元测试,是整个系统的稳定地基 |

---

## 1. 产品概述

**时光小印**是一款桌面专注计时器。用户设定预计用时,系统倒计时并可视化进度;完成后打印一张「预计用时 vs 实际用时」的便签小纸条,帮助用户建立对时间的感知与估算能力。

### 1.1 产品形态

- 桌面设备,外壳 3D 打印(约 7×4.8×12 cm,还原设计图比例)
- 正面圆形可视化表盘(Time-Timer 风格,在 App/网页中渲染,不在设备上有实体屏幕)
- 底部热敏打印机,吐出便签小纸条
- 背面 USB-C 充电口
- 可选搭配:一台电机驱动的经典可视化机械计时器(作为物理前端)

### 1.2 两条打印触发路径(架构核心)

系统存在两条**完全独立**的打印触发路径,共享同一个打印机设备:

```
路径 A — 软件路径:
  App/网页 → 命令 → ESP32 TimerCore(权威时钟)
  → onPrintSlip → TimerCorePrinterBridge → Printer
  → 富便签(预计 / 实际 / 超时)

路径 B — 硬件 hook 路径:
  机芯蓝色信号线(报警时振荡)→ ADC → AlarmDetector
  → EdgeHook → Printer
  → 简单便签(到时消息 + 时间戳)
```

两条路径**互不耦合**。App/网页与 TimerCore 负责"智能"计时体验;物理机芯纯粹作为到时钩子,不参与时间计算,不知道 planned/actual。

### 1.3 设计原则

- **单机必须完整可用**:设备不依赖 WiFi / App / 服务端。WiFi、BLE、服务端全部是叠加增强。
- **TimerCore 是权威时钟**:web/app 是薄 UI(关掉网页/息屏不影响计时和到点打印)。
- **可插拔 I/O**:任何输入来源(WiFi / BLE / 串口调试)都只调用 TimerCore 同一套 event API;任何输出目标(打印机 / WebSocket / BLE notify)都实现同一套 listener 接口注册进去。
- **打印机可选**:无打印机时降级为串口日志;打印机到货后换一个 adapter 实现即可,核心不动。

---

## 2. 硬件规格

### 2.1 主控 — ESP32-S3-N16R8

| 属性 | 值 |
|---|---|
| 型号 | ESP32-S3-N16R8 |
| Flash | 16 MB QIO(足以内嵌完整 web SPA + OTA 双分区) |
| PSRAM | 8 MB OPI(八线);WiFi + BLE + WebServer 同时跑内存充足 |
| 无线 | WiFi 2.4G + **BLE 5.0**(S3 原生,P2 BLE 路径可用) |
| ADC | 使用 **ADC1(GPIO1~10)**;ADC2 在 WiFi 启用后不可用,**禁止用 ADC2** |
| GPIO 电压 | 3.3V;不耐 5V |

**PlatformIO 关键配置:**
```ini
[env:esp32s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
board_build.arduino.memory_type = qio_opi   ; 16MB QIO flash + 8MB OPI PSRAM
board_build.flash_size = 16MB
board_build.partitions = default_16MB.csv
build_flags =
    -std=gnu++17
    -DBOARD_HAS_PSRAM
    ; 串口说明: 若板子只有原生 USB 口 → 保留此行; 若有独立 UART/COM 口且从那里监控 → 删掉
    -DARDUINO_USB_CDC_ON_BOOT=1

[env:native]
platform = native
build_flags = -std=gnu++17
```

### 2.2 物理计时器集成(硬件 hook 路径)

#### 2.2.1 机芯描述

购买的成品可视化计时器,内含:
- 机芯(自带石英振荡 + 减速电机 + 机械拨盘)
- XWS-DJS_V1.1 控制板(蜂鸣器 + LED + 滑动开关 SW1 + 轻触按钮 K1)
- 单节 AA 电池(1.5V)

控制板 SW1 拨到左 = **静音(驱动 LED)**,右 = **蜂鸣**。

#### 2.2.2 5 触点实测数据(参考基准:电池负极)

| 触点 | 颜色 | 功能 | 说明 |
|---|---|---|---|
| 左 1 | 红 | 电池 + (1.5V) | 向机芯供电 |
| 左 2 | 黑 | 电池 − (GND) | 真实共地 |
| 中 | 蓝 | **报警信号** ← 接这根 | 机芯报警触发线 |
| 右 1/2 | — | 向控制板供电/LED | 不接 |

> **注意**:实测到"两根黑线有 1.5V 压差",原因是其中一根线颜色误导,实为 +1.5V 供电线。
> 不要靠线色判断极性,用万用表对电池负极确认。

#### 2.2.3 蓝色信号线电气特性(实测)

| 状态 | 静音档 | 蜂鸣档 | 特征 |
|---|---|---|---|
| 待机 / 关闭 | 1.8V 稳定 | ~3.2V 稳定 | 直流,无振荡 |
| **报警(到时)** | **1.3↔1.8V 跳变** | **2.9↔3.2V 跳变** | **有振荡** |

**判据:振荡(峰峰值),不是绝对电平**——这样静音档和蜂鸣档通吃,SW1 怎么拨都能触发打印。

**为何不接蜂鸣驱动线:** 用户切静音后蜂鸣不响,若接蜂鸣驱动则静音档打印触发失效(静默失效)。蓝线上游于 SW1,静音与否都有振荡信号。

#### 2.2.4 接线方案

```
机芯蓝线 ──┬── R1(10kΩ) ──┬── ESP32 ADC1 脚(GPIO1~10)
           │              │
           │   R2(10kΩ)  │
           └──────────── ─┴── GND(与电池负极共地)
```

÷2 分压作用:
- 将 ~3.2V 压到 ~1.6V,远离 3.3V ADC 上限,防过压和感应尖峰
- ADC 量程内(0~3.1V),无需额外保护
- 静音档最弱振幅(0.5V p-p)÷2 后约 0.25V p-p,仍可被 AlarmDetector 检出

**共地**:ESP32 GND ↔ 电池负极。不需要光耦。

#### 2.2.5 检测参数(校准后填写)

| 参数 | 默认值 | 说明 |
|---|---|---|
| `HOOK_ADC_PIN` | `4`(GPIO4) | 接分压后的 ADC1 脚,待实接后修改 |
| `ALARM_P2P_THRESHOLD` | `300` | 振荡峰峰阈值(ADC counts);用 `calib` 命令实测后调 |
| `ALARM_WINDOW_MS` | `80` | 振荡统计窗口 |
| `HOOK_DEBOUNCE_MS` | `500` | 桥接"嘀嘀"间隙,整段报警只触发一次打印 |
| `ENABLE_PHYSICAL_HOOK` | `0` | 校准完成后改为 `1` 并烧录 |

**校准流程:**
1. 接好分压,`ENABLE_PHYSICAL_HOOK 0`,烧录;
2. 串口敲 `calib`,触发计时器报警;
3. 观察打印的 `p2p` 值(待机 vs 报警对比);
4. 将 `ALARM_P2P_THRESHOLD` 设到两者中间(略低于报警 p2p);
5. 改 `ENABLE_PHYSICAL_HOOK 1`,重烧,验证到时打印一次。

### 2.3 热敏打印机

| 属性 | 值 |
|---|---|
| 规格 | 58mm TTL 热敏打印模块(UART, ESC/POS) |
| 波特率 | 9600 或 115200(查模块手册) |
| 接线 | TX/RX/GND;ESP32 TX → 模块 RX |
| **峰值电流** | **1.5~2A**(打印行时)**— 关键约束** |

**电源注意**:热敏打印瞬时电流达 1.5~2A。必须从充电输入轨或独立升压直接供电,**禁止通过 ESP32 的 3.3V/5V 稳压**供给打印机。打印机供电处加 1000µF+ 去耦电容,防止打印时 ESP32 掉电复位。

### 2.4 电源系统

- LiPo 1500~2000 mAh + TP4056/IP5306 充电管理 + 升压至 5V
- USB-C 充电口(设计图背面)
- 打印机从 5V 升压输出直接供;ESP32 走 LDO 取 3.3V

---

## 3. 软件架构

### 3.1 事件驱动核心 + 可插拔 I/O

```
       ┌─────────────────────────────────────────┐
       │              TimerCore                  │
       │  (状态机,零硬件依赖,确定性,可在 PC 测试) │
       └─────┬───────────────────────────────────┘
             │ ITimerCoreListener
    ┌─────────▼──────────┐    ┌──────────────────────┐
    │ SerialStatus       │    │ TimerCorePrinterBridge│
    │ (调试/串口输出)     │    │ (转交 Printer 设备)   │
    └────────────────────┘    └──────────┬───────────┘
                                         │ Printer 接口
    ┌────────────────────────────────────▼───────────┐
    │                   Printer (设备抽象)            │
    │   PrinterStub (串口降级)  /  ThermalPrinter    │
    └────────────────────────────────────────────────┘
                              ▲ printSimple
    ┌─────────────────────────┴───────────────────────┐
    │  AlarmDetector(振荡检测) → EdgeHook(去抖)       │
    │  (硬件 hook 路径,与 TimerCore 完全独立)         │
    └─────────────────────────────────────────────────┘
```

### 3.2 库结构与关键接口

#### `lib/TimerCore/` — 状态机(已完成,禁止修改)

```cpp
namespace timeprint {

// 状态
enum class State : uint8_t { Idle, Running, Overtime, Paused, Finished };

// 输出效果中携带的数据
struct TickInfo { uint32_t plannedSeconds, elapsedSeconds, remainingSeconds, overrunSeconds; };
struct SlipData { uint32_t plannedSeconds, actualSeconds, overrunSeconds; };

// 输出 adapter 接口:实现此接口并注册到 TimerCore
class ITimerCoreListener {
 public:
  virtual void onStateChanged(State from, State to) {}
  virtual void onTick(const TickInfo& info) {}
  virtual void onPrintSlip(const SlipData& slip) {}
  virtual void onFeedback(Feedback fb) {}  // Start / TimeUp / Stop / Reset
};

// 事件 API(输入,与来源无关)
class TimerCore {
 public:
  bool addListener(ITimerCoreListener*);  // 最多 4 个
  void setTime(uint32_t minutes);   // 仅 Idle 态生效
  void start();   void pause();   void resume();
  void stop();    void reset();
  void tick();    // 推进 1 秒,由 main loop 每 1000ms 调用
};
```

**状态转移:**

```
Idle ──setTime+start──► Running ──(到0)──► Overtime
        ◄──reset──┘      │pause►Paused◄resume│
                          └──stop──► Finished ──► Idle(发 SlipData)
```

#### `lib/Printer/` — 打印机抽象(已完成)

```cpp
class Printer {
 public:
  virtual void printSlip(const SlipData& slip) = 0;   // 软件路径:富便签
  virtual void printSimple(const char* message) = 0;  // 硬件 hook 路径:简单便签
};

// 桥接:TimerCore.onPrintSlip → Printer.printSlip
class TimerCorePrinterBridge : public ITimerCoreListener {
  explicit TimerCorePrinterBridge(Printer* p);
};
```

#### `lib/PhysicalHook/` — 物理 hook 检测(已完成)

```cpp
// 去抖边沿检测:喂入 bool rawLevel + nowMs,在稳定 active 边沿返回 true(一次)
class EdgeHook {
 public:
  EdgeHook(bool activeHigh = false, uint32_t debounceMs = 30);
  bool update(bool rawLevel, uint32_t nowMs);
};

// 振荡检测:喂入 ADC 采样,统计窗口内峰峰值 ≥ 阈值 → 判定为报警振荡
class AlarmDetector {
 public:
  AlarmDetector(int p2pThreshold, uint32_t windowMs);
  bool update(int sample, uint32_t nowMs);
  bool isOscillating() const;
  int lastP2P() const;  // 调试用:上一窗口峰峰值
};
```

**在 main 中的串联:**
```cpp
// loop 中:
int s = analogRead(HOOK_ADC_PIN);
bool osc = alarmDet.update(s, millis());
if (hook.update(osc, millis())) {
    firePhysicalHook();  // → printer.printSimple(...)
}
```

#### `lib/Net/` — 网络控制 adapter(P1 待建)

见 §5 WiFi 设计。需建 `WebControlAdapter`:
- **输入侧**:解析 HTTP REST 命令 + WebSocket 命令 → 调用 `TimerCore` event API
- **输出侧**:实现 `ITimerCoreListener`,将状态序列化为 JSON 通过 WebSocket 推送给所有客户端

---

## 4. 统一通信协议

WiFi(REST/WebSocket)与 BLE(GATT)使用**同一套 JSON schema**,一套逻辑、多种 transport。

### 4.1 命令(Client → Device)

```json
{ "cmd": "set",    "minutes": 25 }
{ "cmd": "start" }
{ "cmd": "pause" }
{ "cmd": "resume" }
{ "cmd": "stop" }
{ "cmd": "reset" }
{ "cmd": "config", "data": { "ssid": "home_wifi", "pass": "xxx", "buzzer": true } }
```

### 4.2 状态推送(Device → Client,WS push / BLE notify)

```json
{
  "state": "running",
  "planned_s": 1500,
  "remaining_s": 842,
  "elapsed_s": 658,
  "overtime_s": 0
}
```

`state` 取值:`"idle"` / `"running"` / `"overtime"` / `"paused"` / `"finished"`

---

## 5. WiFi 子系统(P1 目标)

### 5.1 工作模式

| 模式 | 触发条件 | 说明 |
|---|---|---|
| **AP 模式**(默认) | 无已保存 WiFi | 设备自建热点 `TimePrint-XXXX`;手机直连后弹出 captive portal |
| **STA 模式** | 配网成功后 | 加入家庭 WiFi;局域网内控制 |

### 5.2 技术选型

- **库**: `ESPAsyncWebServer` + `AsyncWebSocket`
- **配网**: captive portal(重定向所有 DNS 请求到配置页);WiFi 凭据存 NVS
- **控制网页**: 单页 SPA,**内嵌进固件 PROGMEM**(16MB flash 足够),不用 SPIFFS 上传
- **实时状态**: WebSocket push,每秒推送一次当前 `TickInfo` JSON
- **API 端点**:
  - `POST /api/cmd` — 接收命令 JSON
  - `GET /api/status` — 返回当前状态 JSON(一次性轮询用)
  - `WS /ws` — 双向:客户端发命令,服务端推状态

### 5.3 SPA 功能(内嵌网页)

- 实时可视化表盘(圆形,红色扇形随 remaining_s 收缩;overtime 切白色)
- 设定时间(数字输入或旋钮控件)
- 开始 / 暂停 / 继续 / 停止 / 重置按钮
- 当前状态 + 超时提示

---

## 6. BLE 子系统(P2 目标)

- **库**: `NimBLE-Arduino`(比默认 BLE 省 ~100KB RAM/Flash,与 WiFi 共存更稳)
- **角色**: GATT Peripheral

| Characteristic | UUID(自定义) | 属性 | 内容 |
|---|---|---|---|
| `CMD` | 自定义 | Write(no-resp) | §4.1 命令 JSON |
| `STATE` | 自定义 | Notify | §4.2 状态 JSON |
| `CONFIG` | 自定义 | Read / Write | 设备配置 JSON |

Service UUID、Characteristic UUID 在 `lib/Net/BLEConfig.h` 中统一定义。

> WiFi 与 BLE 共享 2.4G 单射频。本应用不需要两者同时高吞吐(通常用一个控制),共存无压力。

---

## 7. Android App(P2 目标)

### 7.1 技术栈决策

**Flutter + `flutter_blue_plus`**

选型理由:
- 首次移动开发最平滑(热重载、Dart 上手快)
- BLE 库成熟稳定(这是 App 最大技术风险)
- 未来可扩展到 iOS,无需重写
- Claude Code 对 Flutter 脚手架支持好

### 7.2 连接方式

App 支持三种控制路径(优先级顺序):
1. BLE(蓝牙直连,无需同 WiFi)
2. WiFi STA(局域网 HTTP/WS)
3. WiFi 直连(连设备 AP 热点)

### 7.3 MVP 功能

- 扫描 + 连接(BLE / 选择 WiFi 模式)
- 实时可视化表盘(红→白,动画)
- 设定 / 开始 / 暂停 / 继续 / 停止 / 重置
- 当前状态展示(倒计时数字 + 超时指示)
- 历史记录列表(planned vs actual,估算准确度趋势)

---

## 8. 开发路线图与 Agent 任务说明

### 8.1 P0 — 骨架与核心逻辑(✅ 已完成)

**已建内容:**
- `lib/TimerCore/` — 状态机 + 完整 native 单元测试(9 个场景)
- `lib/Printer/` — `Printer` 接口 + `TimerCorePrinterBridge` + `PrinterStub`
- `lib/PhysicalHook/` — `EdgeHook` + `AlarmDetector` + 各自 native 测试
- `src/main.cpp` — 串口调试台(含 `calib` 校准命令)
- `platformio.ini` — ESP32-S3 + native 两个 env
- 所有 native 测试经 g++ `-Wall -Wextra` 编译并通过

**验证方式:**
```bash
pio test -e native      # PC 上跑所有单元测试
pio run -e esp32s3 -t upload && pio device monitor -b 115200
# 串口敲: set 1 → start → 60s 后 OVERTIME → stop → 打印便签
# 敲 hook → 验证硬件 hook 路径(打印简单便签)
# 敲 calib → 实时打印 ADC/p2p,用于物理 hook 校准
```

---

### 8.2 P1 — Web/WiFi(下一任务)

**目标**: 浏览器能控制 TimerCore,看到实时表盘,无需 App。

**Agent 任务说明:**

新建 `lib/Net/`,包含:

**① `WiFiManager`**(AP/STA 管理):
- 启动时检查 NVS 有无 WiFi 凭据;有则 STA 连接,无则开 AP `TimePrint-XXXX`
- AP 模式:mDNS `timePrint.local`,所有 HTTP 请求重定向到 `/setup`(captive portal)
- `/setup` 页面提交 SSID + 密码 → 存 NVS → 重启进 STA
- 提供 `resetWiFi()` 函数(可从串口 `wifireset` 命令调用)

**② `WebControlAdapter`**(同时做输入 + 输出):
- 实现 `ITimerCoreListener`:在 `onTick` / `onStateChanged` / `onFeedback` 里把状态序列化为 JSON,广播给所有 WebSocket 客户端
- 暴露 `handleCommand(JsonObject)` 方法:将 §4.1 命令翻译成 TimerCore 的 event 调用
- WebSocket 握手时立刻推送当前状态(新客户端打开页面立刻看到)

**③ 内嵌 SPA**:
- 文件放 `src/web/index.html`(含内联 CSS + JS,单文件)
- 构建时用 `xxd -i` 或 PlatformIO build script 转为 C 数组嵌进 `PROGMEM`
- 或直接用 `AsyncWebServer` 的 SPIFFS-free 方式 `server.on("/", HTTP_GET, [](r){ r->send_P(200, "text/html", html_content); })`
- SPA 技术栈:vanilla JS + SVG(不要框架,固件大小可控)

**④ `main.cpp` 修改**:
- 加入 WiFiManager 初始化 + WebControlAdapter 注册为 TimerCore listener
- 加入串口命令 `wifireset`(清除 NVS 凭据,重进 AP 配网模式)

**约束:**
- 不修改 `lib/TimerCore`
- 不修改 `lib/PhysicalHook`
- 保持串口调试命令可用(在 WiFi + 串口两条路径并存)
- WebSocket 推送在 FreeRTOS 任务中或在 loop 中节流(避免 WDT)

---

### 8.3 P2 — Flutter App(P1 完成后)

**目标**: 手机 BLE/WiFi 控制 + 实时可视化表盘。

**Agent 任务说明:**

`app-flutter/` 目录:

**固件侧新增** `lib/Net/BLEAdapter`:
- 使用 `NimBLE-Arduino`
- 实现与 P1 `WebControlAdapter` 对称的接口:输入(收 BLE GATT write → TimerCore event)+ 输出(TimerCore listener → BLE notify)
- Service + Characteristic UUID 在 `lib/Net/BLEConfig.h` 统一定义

**App 侧:**
- Flutter 3.x + `flutter_blue_plus`
- BLE 优先连接;降级到 HTTP/WS(WiFi)
- 表盘 Widget:用 `CustomPainter` 绘制 Time-Timer 风格圆形(红色扇形 → 超时切白色)
- 接收 WS/BLE notify 的状态 JSON,驱动 `AnimatedBuilder` 做流畅动画
- 命令发送:BLE write / HTTP POST 复用同一套 `TimerService` 抽象

---

### 8.4 P3 — 热敏打印机(打印机到货后)

**目标**: 替换 `PrinterStub`,实现真实打印。

**Agent 任务说明:**

新建 `lib/Printer/ThermalPrinter.h/.cpp`:
- 继承 `Printer`,通过 UART(ESP32 `Serial1` 或 `Serial2`)发送 ESC/POS 指令
- `printSlip(SlipData)`:排版「预计 / 实际 / 超时 / :)」,宽度适配 58mm(约 32 字符/行)
- `printSimple(message)`:打印消息 + uptime + `:)`
- 处理打印机初始化(ESC @)、走纸、切纸(若有)

**在 `main.cpp` 中**: 将 `PrinterStub` 替换为 `ThermalPrinter` 实例,传入正确的 `HardwareSerial&` 和波特率。

**电源**:确认打印机供电从 5V 升压轨取,有足够的滤波电容,与 ESP32 供电分离。

---

### 8.5 P4 — 物理 hook 接线调试

**目标**: 机芯到时 → 自动触发打印。

**步骤(开发者操作,非 agent 任务):**
1. 按 §2.2.4 接好蓝线 ÷2 分压到某 ADC1 脚
2. 固件烧录(`ENABLE_PHYSICAL_HOOK 0`),串口 `calib` 采集报警时 `p2p` 值
3. 将 `ALARM_P2P_THRESHOLD` 设为待机 p2p × 2(在待机噪声和报警 p2p 之间)
4. 改 `HOOK_ADC_PIN`、`ENABLE_PHYSICAL_HOOK 1`,重烧
5. 触发机芯报警,验证打印一次、不重复

---

### 8.6 P5 — 打磨(最后阶段)

- **历史记录**: LittleFS 中追加 `{ts, planned_s, actual_s, overrun_s}` JSON;App 拉取做估算准确度图表
- **NTP 时间戳**: STA 模式下同步时间,便签印真实时刻
- **OTA 固件**: ArduinoOTA 或 HTTP OTA,设备不拆机更新
- **可选服务端**: FastAPI + PostgreSQL,多设备历史同步;设备无服务端必须完整可用
- **3D 外壳**: 还原设计图比例(约 7×4.8×12 cm);PrintFarm Bambu Lab 打印

---

## 9. 仓库结构

```
timeprint/
├── firmware/                    # PlatformIO 工程
│   ├── platformio.ini
│   ├── lib/
│   │   ├── TimerCore/           # ✅ 已完成。禁止修改。
│   │   │   ├── TimerCore.h
│   │   │   └── TimerCore.cpp
│   │   ├── Printer/             # ✅ 已完成
│   │   │   ├── Printer.h        # 设备接口
│   │   │   └── TimerCorePrinterBridge.h
│   │   ├── PhysicalHook/        # ✅ 已完成
│   │   │   ├── EdgeHook.h/cpp
│   │   │   └── AlarmDetector.h/cpp
│   │   └── Net/                 # ← P1/P2 在这里建
│   │       ├── WiFiManager.h/cpp
│   │       ├── WebControlAdapter.h/cpp
│   │       ├── BLEConfig.h      # UUID 定义
│   │       └── BLEAdapter.h/cpp
│   ├── src/
│   │   ├── main.cpp
│   │   └── web/
│   │       └── index.html       # ← P1 内嵌 SPA 源文件
│   └── test/
│       ├── test_core/           # ✅
│       ├── test_hook/           # ✅
│       └── test_alarm/          # ✅
├── app-flutter/                 # ← P2 Flutter App
├── server/                      # ← P5 可选 FastAPI 服务端
├── hardware/                    # 原理图 / BOM / 魔改接线记录 / 3D 外壳文件
└── docs/
    └── TimePrint_Spec_v1.0.md   # 本文件
```

---

## 10. 关键技术约束与坑点(必读)

1. **ADC2 禁用**: ESP32-S3 的 ADC2 在 WiFi 启用后无法使用。蓝线和一切 ADC 读取必须用 **ADC1(GPIO1~10)**。

2. **热敏打印机电流**: 打印时峰值 1.5~2A。绝对不能从 ESP32 稳压供给。必须从 5V 升压轨直取,加 1000µF+ 去耦电容。否则打印时 ESP32 会复位。

3. **ESP32-S3 USB-CDC**: S3 有原生 USB CDC。若板子只有一个 USB 口,`Serial` 须走 CDC(需 `-DARDUINO_USB_CDC_ON_BOOT=1`)。若有独立 UART/COM 口并从那里监控,反而要去掉这个 flag。症状:烧录成功但串口无输出。

4. **NimBLE vs 默认 BLE**: 必须用 `NimBLE-Arduino`,不用 ESP32 默认 BLE 库。原因:节省约 100KB RAM/Flash;WiFi+BLE 共存更稳定。

5. **PSRAM 启用**: `board_build.arduino.memory_type = qio_opi` 是启用 8MB OPI PSRAM 的关键。漏掉此行则 PSRAM 不可用。

6. **蓝线分压**: 蓝线最高约 3.2V,ESP32-S3 ADC 上限约 3.1~3.3V。必须加 ÷2 分压,否则会裁剪(clipping),振荡峰峰值失准,`AlarmDetector` 误判。

7. **AlarmDetector 阈值**: 默认 300 counts 需用 `calib` 命令实测后调整。ESP32 ADC 低端非线性,不同批次 ADC 偏差可达 ±10%,必须在目标硬件上校准。

8. **SPA 内嵌**: P1 网页直接内嵌进 PROGMEM(不上传文件系统)。SPA 不用任何 npm 构建工具,纯 vanilla JS + inline SVG,保证固件自包含。

9. **TimerCore 是权威时钟**: App/网页永远不要在客户端自己跑倒计时逻辑。关标签/息屏不影响计时;多端同时连接看到同一个倒计时。所有时间数据从 ESP32 推送的 `TickInfo` 里读。

10. **hook 路径与 TimerCore 完全独立**: 物理机芯触发打印时不要去调用 `TimerCore.stop()`。两条路径独立,各管各的。混淆会导致一次到时触发两次打印。

---

## 11. 便签格式参考

**软件路径(富便签)** — `printSlip(SlipData)`:
```
──────────────────────
  预计 planned : 25:00
  实际 actual  : 28:34
  超时 overrun : 03:34
        :)
──────────────────────
```

**硬件 hook 路径(简单便签)** — `printSimple(message)`:
```
──────────────────────
  物理计时器到时
  2025-xx-xx 14:30
        :)
──────────────────────
```

---

*文档版本:v1.0 | 状态:P0 已完成,P1 待建*
