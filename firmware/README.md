# TimePrint 固件

这是 ESP32-S3 TimePrint 计时/打印设备的正式固件工程。

## 技术栈

- PlatformIO + Arduino
- ESP32-S3-N16R8
- C++17
- 面向硬件无关逻辑的 native Unity 测试
- 从 PROGMEM 提供服务的内嵌原生 JavaScript/SVG 控制页

## 命令

串口监视器命令：

```text
set <min>
start
pause
resume
stop
reset
hook
calib
status
wifireset
help
```

HTTP/WebSocket 命令 JSON：

```json
{ "cmd": "set", "minutes": 25 }
{ "cmd": "start" }
{ "cmd": "pause" }
{ "cmd": "resume" }
{ "cmd": "stop" }
{ "cmd": "reset" }
{ "cmd": "config", "data": { "ssid": "home_wifi", "pass": "secret" } }
```

状态 JSON：

```json
{
  "state": "running",
  "planned_s": 1500,
  "remaining_s": 842,
  "elapsed_s": 658,
  "overtime_s": 0
}
```

## 验证

```bash
pio test -e native
pio run -e esp32s3
pio run -e esp32s3 -t upload --upload-port COM3
pio device monitor -b 115200
```

如果当前 shell 中没有 `pio`，优先使用 `python -m platformio` 运行等价命令；两者都不可用时，再记录为验证缺口。

## 硬件说明

- 默认打印机是 `PrinterStub`；它会把便签内容输出到串口，因此无需 HPD482 硬件也能验证固件行为。
- `HPD482Printer` 是真实打印机驱动基础。它使用 UART 115200，等待 `READY`，配置 `AT+EC=2`，并带超时发送 HPD482 AT 指令。当前默认尚未启用。
- 物理 hook 默认关闭。启用 `ENABLE_PHYSICAL_HOOK` 前，必须先把物理计时器蓝色信号线经分压接入 ADC1 GPIO，并通过 `calib` 完成校准。
- 真实打印机驱动必须面向 HPD482 AT 指令，不得按 ESC/POS 假设实现。
