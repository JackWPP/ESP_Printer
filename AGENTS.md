# TimePrint 项目代理约定

本文件约束整个仓库。执行任务时，必须同时遵守更高优先级的系统/开发者指令。

## 项目方向

- 正式固件位于 `firmware/`。
- `Explore_Codes/` 只作为参考材料。不得直接把其中代码作为最终实现发布；如需采用其中思路，必须先重建到正式工程结构中，再完成审查和测试。
- 固件技术栈固定为 ESP32-S3-N16R8 上的 PlatformIO + Arduino，语言标准为 C++17。
- ESP-IDF 路径、本机 VSCode 配置等都属于开发者机器细节，不得成为正式构建的必要条件。
- 第一版 Web 界面是固件内嵌的单页应用，使用原生 JavaScript、内联 CSS 和 SVG。除非明确要求，不要为固件界面引入 npm 工具链。

## 固件架构

- `lib/TimerCore` 是权威计时状态机，必须保持硬件无关、行为确定。
- 所有控制输入，包括串口、HTTP、WebSocket 和未来 BLE，都调用同一套 `TimerCore` 事件 API。
- 所有输出适配器都使用 listener 风格边界，不得把网络、打印机或 UI 代码塞进 `TimerCore`。
- 软件计时路径与物理 hook 路径相互独立：物理 hook 检测不得调用 `TimerCore.stop()`，也不得推断计划时间或实际计时数据。
- 物理计时器 hook 的 ADC 工作只能使用 ADC1 GPIO。WiFi 启用后不得使用 ADC2。

## 打印机规则

- 打印机硬件真相以 `docs/` 中的 HPD482E-32 文档为准。
- 真实打印机驱动必须使用 UART 115200，等待 `READY\r\n`，发送 AT 指令，并使用明确超时。除非硬件更换，不得按 ESC/POS 假设实现。
- 打印机需要独立 12V/2A 供电，并与 ESP32 共地。严禁从 ESP32 3.3V、ESP32 5V 或仅 USB 供电轨给打印机供电。
- 在真实打印机集成前，默认保持 `PrinterStub`，确保无硬件时固件仍可测试。

## Git 纪律

- 提交必须小而可审查，并绑定到一个可验证里程碑。
- 每完成一个已验证里程碑就提交一次：项目骨架、P0 核心/测试、WiFi/Web 基础，以及后续硬件集成。
- 每个提交信息必须使用根指令中的 Lore 协议：第一行写变更意图，正文后使用有价值的 trailer，例如 `Tested:` 和 `Not-tested:`。
- 提交固件改动前，运行当前最强可用验证：逻辑改动至少跑 native 测试，固件集成改动还要跑 ESP32-S3 编译。
- 不得提交构建产物、PlatformIO 缓存、运行态 `.omx/`、串口日志或本机绝对路径 IDE 配置。

## 验证基线

- native 逻辑：`cd firmware && pio test -e native`
- 固件编译：`cd firmware && pio run -e esp32s3`
- 确认端口后烧录设备：
  `cd firmware && pio run -e esp32s3 -t upload --upload-port COM3`
- 串口监视：`cd firmware && pio device monitor -b 115200`

如果 shell 中没有 `pio`，优先使用 `python -m platformio` 运行等价命令；两者都不可用时，必须记录为验证缺口，不要声称 PlatformIO 验证已通过。
