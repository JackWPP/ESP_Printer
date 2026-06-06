# TimePrint 台架联调检查清单

从固件骨架进入台架集成时，按本清单逐项验证。

## 1. ESP32-S3 基线

- 将开发板连接到 COM3。
- 在 `firmware/` 下编译、烧录并打开串口：

```powershell
python -m platformio run -e esp32s3
python -m platformio run -e esp32s3 -t upload --upload-port COM3
python -m platformio device monitor -b 115200 --port COM3
```

- 预期串口启动信息：
  - `=== TimePrint 固件骨架 ===`
  - `AP TimePrint-XXXX 地址 192.168.4.1`
  - `Web 服务已启动`
  - `打开 http://192.168.4.1/`
- 在仓库根目录运行自动检查：

```powershell
python tools/smoke/serial_boot_smoke.py --port COM3
```

## 2. Web 控制

- 从电脑连接 `TimePrint-XXXX` WiFi AP。
- 打开 `http://192.168.4.1/`。
- 可选：通过串口把开发板配置到本地 2.4 GHz 局域网：

```powershell
python tools/smoke/serial_wifi_provision.py --port COM3 --ssid "<ssid>" --password "<password>"
```

- 配网后，使用脚本打印出的 STA IP 替代 `192.168.4.1`。
- 验证命令：
  - 设置分钟数并启动。
  - 暂停并继续。
  - 停止，并确认串口出现 `PrinterStub` 便签。
  - 重置，并确认状态回到 idle（待机）。
- 如需直接验证 HTTP：

```powershell
Invoke-RestMethod -Uri http://192.168.4.1/api/status
Invoke-RestMethod -Method Post -Uri http://192.168.4.1/api/cmd `
  -ContentType application/json -Body '{"cmd":"set","minutes":1}'
```

- 加入 AP 后运行自动检查：

```powershell
python tools/smoke/http_control_smoke.py --base-url http://192.168.4.1
```

## 3. HPD482 打印机

- 打印机必须使用独立 12V/2A 电源。
- 不要把打印机 VCC 接到 ESP32 电源轨。
- 打印机 GND 接 ESP32 GND。
- 打印机 TXD 接 ESP32 RX，打印机 RXD 接 ESP32 TX。
- 线尽量短；如果 UART 影响启动或烧录，烧录时先断开打印机 UART。
- 先做独立检查：
  - 等待 `READY\r\n`。
  - 发送 `AT`，预期 `OK\r\n`。
  - 发送 `AT+EC=2`，预期 `EC OK\r\n`。
  - 发送简单 UTF-8 文本，预期 `PT OK\r\n`。
- 独立检查通过后，才允许在 `src/main.cpp` 中用 `HPD482Printer` 替换 `PrinterStub`。

## 4. 物理计时器 Hook 路径

- 蓝色计时器报警信号必须经 10k/10k 分压接入 ADC1 GPIO。
- ESP32 GND 与计时器电池负极共地。
- WiFi 启用时不要使用 ADC2。
- 校准前保持 `ENABLE_PHYSICAL_HOOK` 关闭。
- 运行串口 `calib`，触发物理报警，并记录：
  - 待机 ADC 值
  - 待机 p2p
  - 报警 ADC 范围
  - 报警 p2p
- 只有完成校准后，才能设置 `HOOK_ADC_PIN` 和 `ALARM_P2P_THRESHOLD`。
- 启用 `ENABLE_PHYSICAL_HOOK` 后，验证每次报警事件只打印一张简单便签。
