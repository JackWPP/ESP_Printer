# TimePrint Firmware — P0 骨架

ESP32 智能可视化计时器固件。当前为 **P0**:可移植的 `TimerCore` 状态机 + native 单元测试 + 串口命令调试台。
**无需任何外设(连打印机都不用)即可验证全部计时逻辑。**

## 目录结构
```
timeprint/
├── platformio.ini          # esp32s3(固件) + native(PC 测试) 两个 env
├── lib/
│   ├── TimerCore/          # 纯逻辑状态机(软件计时路径),零硬件依赖
│   ├── Printer/            # Printer 设备接口 + TimerCorePrinterBridge
│   └── PhysicalHook/       # EdgeHook(去抖)+ AlarmDetector(振荡检测),可移植
├── src/main.cpp            # 串口调试台;两条打印路径汇到 PrinterStub
└── test/
    ├── test_core/          # TimerCore 单元测试(Unity)
    ├── test_hook/          # EdgeHook 单元测试(Unity)
    └── test_alarm/         # AlarmDetector 单元测试(Unity)
```

## 跑起来

**① PC 上跑核心逻辑测试(不需要板子):**
```bash
pio test -e native
```

**② 烧进 ESP32,打开串口调试台:**
```bash
pio run -e esp32s3 -t upload
pio device monitor -b 115200
```
然后在串口里敲(回车执行):
```
help
set 1      # 设 1 分钟
start      # 开始,每秒打印 [TICK]
           # 60s 后自动进入 OVERTIME(红->白,触发 TIME-UP)
stop       # 停止,打印便签:planned / actual / overrun(软件路径)
hook       # 模拟机芯到时,验证硬件 hook 的打印路径
calib      # 切换校准模式:实时打印 ADC 值/峰峰值,用来定报警阈值
```

> **板子**:已配置为 **ESP32-S3-N16R8**(16MB QIO flash + 8MB OPI PSRAM,WiFi + BLE 5.0)。
> 若烧录成功但串口无输出,看 `platformio.ini` 里 `ARDUINO_USB_CDC_ON_BOOT` 那段注释(原生 USB 口 vs 独立 UART 口的取舍)。
> `TimerCore` 与 native 测试和型号无关。

## 串口命令
`set <min>` · `start` · `pause` · `resume` · `stop` · `reset` · `hook` · `calib` · `status` · `help`

## 架构要点:两条打印路径,一个打印机
- **软件路径**:`TimerCore`(设/启/暂停/倒计时/超时/停止,由 web/app 控制)
  → `onPrintSlip` → `TimerCorePrinterBridge` → `Printer.printSlip`(富便签:预计/实际/超时)。
  **`TimerCore` 是权威时钟**,web/app 只是它的薄 UI(关页面/息屏不影响计时与到点打印)。
- **硬件 hook 路径**:机芯**蓝色信号线**(报警时振荡)→ ADC → `AlarmDetector`(检测振荡)
  → `EdgeHook`(去抖)→ `Printer.printSimple`(简单便签)。机芯只当"到点就触发"的钩子,
  **完全绕过 TimerCore**,读不到也不需要 planned/actual。
- 两条路径都通过 `Printer` 接口打印,互不耦合。打印机到货后只需新建 `ThermalPrinter : Printer`。
- `TimerCore` 不依赖 wall-clock:`tick()` 推进 1 秒,逻辑可确定性测试。

## 机芯 hook 接线(实测结论)
机芯单节供电,信号以**电池负极为共地**测量:蓝线待机稳定直流(静音档 1.8V / 蜂鸣档 ~3.2V),
**报警时跳变**(静音 1.3↔1.8V,蜂鸣 2.9↔3.2V)。判据是**"在不在振荡"**,与档位无关 —— 这样
**用户静音也能触发打印**(若改接蜂鸣驱动,静音档就不响、不触发了)。
- 接法:蓝线 → **÷2 分压(10k/10k)** → ESP32 的 **ADC1 脚(GPIO1~10**,ADC2 在 WiFi 下不可用**)**;与电池负极共地。分压把 ~3.2V 压低,防过压/防尖峰,且远离 ADC 上限。
- 不需要放大、不需要光耦(共地已确认;幅度 1.3~3.2V 足够)。
- 标定:接好线后,串口 `calib` 打开校准,读报警时的 `p2p` 值,据此设 `ALARM_P2P_THRESHOLD`,
  再把 `main.cpp` 顶部 `ENABLE_PHYSICAL_HOOK` 改成 `1`、`HOOK_ADC_PIN` 改成实际引脚。

## 路线图(已按 WiFi-first 重排)
- [x] **P0** core + Printer 接口 + EdgeHook + AlarmDetector + native 测试 + 串口台
- [ ] **P1 Web/WiFi** AP 直连 + STA 配网 + 控制网页(REST + WebSocket 实时状态)
- [ ] **P2 App** Flutter,支持 BLE / WiFi / WiFi 直连
- [ ] **P3 打印机** 串口热敏 `ThermalPrinter : Printer`(到货后替换 `PrinterStub`)
- [ ] **P4 机芯 hook 接线** 蓝线 ÷2 分压 → ADC1 → `calib` 标定阈值 → 开启 `ENABLE_PHYSICAL_HOOK`
- [ ] **P5 打磨** 历史 / NTP 时间戳 / OTA / 可选服务端 / 3D 外壳

## P1 给 Claude Code 的提示
> 目标:让 `TimerCore` 能被浏览器控制,且不碰核心。
- 新建 `lib/Net/`:用 `ESPAsyncWebServer` 起 AP `TimePrint-XXXX` + captive portal 配网(存 NVS),支持 STA。
- 新建 `WebControlAdapter`:**同时**做输入(收 WS/REST 命令调用 core)和输出(实现 `ITimerCoreListener`,把状态以 JSON 通过 WebSocket 推给前端)。
- 内嵌一个单页前端,用收到的状态 JSON 绘制可视化表盘(红→白)。
- 命令/状态 JSON schema 见主 spec(`TimePrint_Spec_v0.2.md`)§8。
- **约束:不要修改 `lib/TimerCore`。** 它已通过单元测试,是稳定地基。
