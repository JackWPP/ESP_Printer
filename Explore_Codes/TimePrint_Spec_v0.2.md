# 时光小印 · TimePrint — 技术 Spec v0.2

> 基于 ESP32 的智能可视化计时器。把「轻量型时间管理 + 情绪关怀」的产品概念落地。
> **v0.2 变更**:确认设备唯一硬件 IO = 串口热敏打印机,**无屏幕 / 无 LED**;可视化交给 App 与物理表盘;物理计时器作为**可选旁观式输入**接入;安卓栈定为 Flutter。

---

## 1. 项目概述

| 项 | 内容 |
|---|---|
| 形态 | 一个 headless ESP32 智能模块(无屏无灯)+ 串口热敏打印机;搭配 App 控制与可视化 |
| 物理表盘 | (可选)一台电机驱动的经典可视化计时器,作为漂亮的实体显示 + 旋钮输入,经魔改接入 |
| MCU | ESP32(WiFi + BLE) |
| 唯一硬件 IO | **串口(UART)热敏打印机** |
| 供电 | LiPo + USB-C 充电 |
| 控制方式 | ① App(BLE / WiFi)为主 ② 物理计时器旋钮(魔改后,可选) ③ 调试期可选 LED/串口 |
| 设计原则 | **单机 + App 必须完整可用**;物理表盘魔改、服务端都是叠加增强 |

---

## 2. 核心交互模型

沿用「时光小印」交互理念,但拆成两层:

**逻辑层(ESP32 + App,MVP 必做):**
1. App(或物理旋钮)设定预计用时 → `START`
2. ESP32 自己计时,App 实时显示倒计时表盘(红盘收缩)
3. 超过预计时间进入超时态(App 切红→白盘),继续计超时
4. 用户确认完成 → `STOP`
5. ESP32 记录 预计用时 / 实际用时 / 超时量 → 串口打印便签(含笑脸)→ 入历史

**物理层(可选,魔改后):**
- 物理计时器照常做机械可视化(电机转盘 + 自带蜂鸣)
- ESP32 旁观读取它的状态,把"物理操作"也变成逻辑层的 event

---

## 3. 系统架构(核心未变,I/O 简化)

**事件驱动核心 + 可插拔 I/O。** 砍掉 v0.1 的屏幕显示后更轻。

### 3.1 核心:`TimerCore`(可移植纯逻辑,ESP32 为计时主体)
- 状态:`IDLE / SETTING / RUNNING / OVERTIME / PAUSED / FINISHED / PRINTING`
- 输入事件(与来源无关):`SET_TIME(min)`、`START`、`PAUSE`、`RESUME`、`STOP`、`RESET`、`TICK(1s)`
- 输出效果(与去向无关):`broadcastState(...)`、`feedback(...)`、`printSlip(planned_s, actual_s, overrun_s)`
- 可在 PC 上跑 native 单元测试,零硬件依赖。

### 3.2 输入 adapters(只负责发 event)
- **App-WiFi**:HTTP REST + WebSocket
- **App-BLE**:GATT characteristic 写入
- **物理计时器(可选,Phase 5)**:
  - 电位器抽头 → ADC → `SET_TIME` + 实时进度
  - 蜂鸣驱动脚 → GPIO → `STOP`/"到时"触发
- **调试(可选)**:串口命令 / 一颗 LED 看状态

### 3.3 输出 adapters(只负责消费 effect)
- **Printer(唯一硬件输出)**:UART 热敏打印,ESC/POS;**未接打印机时降级为串口 printf**
- **Status broadcast**:WebSocket push(网页)+ BLE notify(App)→ App 端绘制表盘
- **Buzzer(可选)**:无源蜂鸣;若已接物理计时器,可复用它自带的蜂鸣
- **LED(仅调试)**:中间形态看状态用,正式产品不需要

> 解耦收益:no-op 打印 adapter 让你没打印机也能调通核心;砍屏幕只是换掉一个 output adapter,核心零改动;物理表盘魔改后只是新增一个 input adapter。

---

## 4. 功能需求

| ID | 需求 |
|---|---|
| FR1 | ESP32 自计时,App 设定/开始/暂停/停止全流程 |
| FR2 | App 端绘制可视化表盘(红→白超时切换);物理表盘(可选)做实体可视化 |
| FR3 | 完成后串口打印「预计 vs 实际」便签;**无打印机时降级为串口日志** |
| FR4 | WiFi:AP 模式(自带控制网页,无路由器可用) + STA 配网 |
| FR5 | BLE GATT 控制 + Flutter 安卓 App |
| FR6 | (可选)服务端:历史同步 / 远程 / OTA;非必需 |
| FR7 | LiPo + USB-C 充电 |
| FR8 | 本地记录历史(预计 vs 实际),供估算准确度统计 |
| FR9 | (可选)物理计时器旁观式接入,把实体旋钮/到时变成 event |

---

## 5. 硬件选型 / BOM(MVP 极简)

| 子系统 | 方案 | 备注 |
|---|---|---|
| MCU | ESP32(**需确认型号**,BLE 需 classic/C3/S3,非 S2) | 你已有 |
| 唯一 IO | 58mm TTL 热敏打印模块(UART,ESC/POS) | 峰值电流大,见 §13 |
| 供电 | LiPo 1500–2000mAh + 充电管理 + 升压 5V,USB-C | 打印峰值 1.5–2A |
| 蜂鸣(可选) | 无源蜂鸣片 | 或复用物理计时器自带蜂鸣 |
| 调试 LED(可选) | 单色/WS2812 一颗 | 仅中间形态用 |
| 外壳 | 3D 打印(PrintFarm) | |

> v0.1 的 GC9A01 圆屏 / LVGL **已移除**。

**物理计时器(独立可选配件,Phase 5 接入):** 见 §6,改动是给它焊两根偷听线 + 共地。

---

## 6. 物理计时器:推断的内部结构 + 魔改方案

### 6.1 推断(待拆机验证)
特征:插电池、旋出去的部分随时间复位、像小闹钟 → **电机驱动的电子可视化计时器**。
- 供电:1~2 节 AA/AAA(1.5~3V)
- 主控:COB 黑胶 MCU(黑盒)
- **旋钮位置:大概率旋转电位器**(便宜的闭环位置反馈,才能精确转回 0)
- 电机:小直流减速电机,把色盘转回 0
- 蜂鸣器:到 0 报警

### 6.2 旁观式魔改(非侵入,推荐)
ESP32 不驱动它,只偷听两路:
1. **电位器中间抽头 → ESP32 ADC**:得到 设定时间 + **实时剩余进度**(电机在转,分压持续变化)
2. **蜂鸣驱动脚 → ESP32 GPIO**:到时触发 → 打印
- 必须 **共地**;注意电池仅 1.5~3V,蜂鸣脚可能需分压/光耦隔离;ESP32 ADC 噪声大,需过采样滤波。

### 6.3 拆机验证清单
- 旋钮后 3 脚件:外侧两脚阻值固定、中脚随旋转变化 → 电位器;记电压范围
- 找到 2 线电机
- 报警时量蜂鸣驱动脚电压/极性
- 记电池电压

### 6.4 兜底(若是开环、无电位器)
- 只能拿"到时"脉冲 + 可能的"启动"信号;设定时间改由 App 给,或自己在旋钮轴加编码器。
- **MVP 不依赖魔改**:先做 headless 智能计时器,物理接入是后期增强。

---

## 7. 固件技术栈

- PlatformIO + Arduino-ESP32,FreeRTOS task 做并发(WiFi / BLE / 计时 / 打印)
- WiFi:ESPAsyncWebServer(REST + WebSocket)+ captive portal 配网
- BLE:**NimBLE-Arduino**(省 RAM,和 WiFi 共存好)
- JSON:ArduinoJson
- 打印:Adafruit_Thermal 或自写 ESC/POS
- 持久化:NVS 存配置;LittleFS 存历史 JSON

---

## 8. 统一通信协议(WiFi 与 BLE 共用)

**命令(Client → Device):**
```json
{ "cmd": "set", "minutes": 25 }
{ "cmd": "start" }
{ "cmd": "pause" }
{ "cmd": "resume" }
{ "cmd": "stop" }
{ "cmd": "reset" }
{ "cmd": "config", "data": { "ssid": "...", "pass": "...", "buzzer": true } }
```

**状态(Device → Client,WS/BLE notify):**
```json
{ "state": "running", "planned_s": 1500, "remaining_s": 842, "elapsed_s": 658, "overtime_s": 0 }
```

App 端用这套状态自行绘制可视化表盘。

---

## 9. WiFi 模式设计

- **AP 模式(单机)**:无已存网络则开热点 `TimePrint-XXXX`,captive portal 弹出控制+配置页,无路由器可用。
- **STA 模式**:配网后局域网内由网页/App 控制(同一套 REST+WS)。
- **控制网页**:内嵌固件单页,WS 实时状态 + 设定/开始/停止 + 历史。

---

## 10. BLE 设计(GATT)

自定义 Service,三个 characteristic:`CMD`(Write,收 §8 命令)、`STATE`(Notify,推 §8 状态)、`CONFIG`(R/W)。与 WiFi 端复用同一 JSON 逻辑。

---

## 11. 安卓 App —— 技术栈:**Flutter(已定)**

- 库:`flutter_blue_plus`(BLE)
- MVP 功能:扫描连接 → 设定/开始/停止 → **实时可视化表盘(红→白)** → 历史曲线(预计 vs 实际)
- 选 Flutter 理由:首次移动开发最平滑、BLE 库成熟可靠(BLE 稳定性是本 App 头号风险)、一套代码可扩 iOS、Claude Code 脚手架支持好。备选 React Native(复用 JS,但 BLE 稳定性略逊)。

---

## 12. 服务端(可选,Phase 6+)

- 仅用于:云历史同步 / 远程控制 / OTA / 多设备。栈:FastAPI + PostgreSQL(+ 可选 MQTT)。
- 铁律:设备脱离服务端必须完整可用。

---

## 13. 电源设计要点

- 热敏打印瞬时 **1.5–2A**;打印机供电走独立大电流 5V 升压或充电输入轨,加 1000µF+ 去耦,防打印时 MCU 掉电复位。

---

## 14. 数据与持久化

- 配置:NVS。历史:LittleFS JSON 追加 `{ts, planned_s, actual_s, overrun_s}`,供 App 做估算准确度统计(产品核心价值)。

---

## 15. 开发路线图(Claude Code 友好)

| Phase | 目标 | 验证 |
|---|---|---|
| **P0 骨架** | `TimerCore` 状态机 + native 单元测试;串口命令驱动状态(打印=串口空实现) | FR1、FR3 降级 |
| **P1 协议+BLE** | 统一命令/状态 schema;NimBLE GATT;串口可驱动全流程 | FR5 雏形 |
| **P2 WiFi** | AP captive portal 配网 + STA + 控制网页(REST+WS) | FR4 |
| **P3 Flutter App** | 扫描连接 + 控制 + 实时表盘绘制 + 历史 | FR2、FR5 |
| **P4 打印** | 热敏打印 adapter + 便签排版(预计/实际/笑脸)+ 电源加固 | FR3 |
| **P5 物理表盘魔改** | 拆机验证 → 电位器 ADC + 蜂鸣触发 旁观接入 | FR9 |
| **P6 打磨** | 历史统计 / OTA / 可选服务端 / 3D 外壳 / 电池集成 | FR6-8 |

> P1 移除了屏幕,改为先打通协议层;P2/P3 可并行;打印模块到货后随时插入。

---

## 16. 仓库结构建议

```
timeprint/
  firmware/            # PlatformIO
    src/
      core/            # TimerCore(可移植,无硬件依赖)
      hal/             # printer / buzzer / (physical-timer) adapters
      net/             # wifi / ble / web / protocol
      main.cpp
    test/              # native 单元测试
    platformio.ini
  app-flutter/         # Flutter 安卓 App
  server/              # 可选 FastAPI
  hardware/            # 原理图 / BOM / 物理计时器魔改记录 / 3D 外壳
  docs/                # 本 spec
```

---

## 17. 待确认

> 已替你定的:安卓栈 = Flutter;砍掉屏幕;物理表盘为可选旁观接入。剩一个硬阻塞项:

1. **ESP32 型号?**(BLE 需 classic/C3/S3,**S2 无蓝牙**)拍个板子丝印或报型号即可。

确认型号后即可开 **P0**:`TimerCore` + native 测试 + 串口驱动的完整 PlatformIO 工程框架。
