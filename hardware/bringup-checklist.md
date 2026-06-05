# TimePrint Bring-up Checklist

Use this checklist when moving from firmware scaffold to bench integration.

## 1. ESP32-S3 Baseline

- Connect the board on COM3.
- Build and upload from `firmware/`:

```powershell
python -m platformio run -e esp32s3
python -m platformio run -e esp32s3 -t upload --upload-port COM3
python -m platformio device monitor -b 115200 --port COM3
```

- Expected serial startup:
  - `=== TimePrint firmware scaffold ===`
  - `AP TimePrint-XXXX at 192.168.4.1`
  - `Web server started`
  - `open http://192.168.4.1/`
- Automated check from the repository root:

```powershell
python tools/smoke/serial_boot_smoke.py --port COM3
```

## 2. Web Control

- Join the `TimePrint-XXXX` WiFi AP.
- Open `http://192.168.4.1/`.
- Optional: provision the board onto the local 2.4 GHz LAN over serial:

```powershell
python tools/smoke/serial_wifi_provision.py --port COM3 --ssid "<ssid>" --password "<password>"
```

- After provisioning, use the printed STA IP instead of `192.168.4.1`.
- Verify commands:
  - Set minutes and start.
  - Pause and resume.
  - Stop and confirm a serial `PrinterStub` slip.
  - Reset and confirm state returns to idle.
- Verify HTTP directly if needed:

```powershell
Invoke-RestMethod -Uri http://192.168.4.1/api/status
Invoke-RestMethod -Method Post -Uri http://192.168.4.1/api/cmd `
  -ContentType application/json -Body '{"cmd":"set","minutes":1}'
```

- Automated check after joining the AP:

```powershell
python tools/smoke/http_control_smoke.py --base-url http://192.168.4.1
```

## 3. HPD482 Printer

- Power printer from its required independent 12V/2A supply.
- Do not connect printer VCC to ESP32 power rails.
- Connect GND to ESP32 GND.
- Connect printer TXD to ESP32 RX and printer RXD to ESP32 TX.
- Keep wires short; disconnect printer UART while uploading if the UART line
  interferes with boot/upload.
- First standalone checks:
  - Wait for `READY\r\n`.
  - Send `AT`, expect `OK\r\n`.
  - Send `AT+EC=2`, expect `EC OK\r\n`.
  - Send simple UTF-8 text, expect `PT OK\r\n`.
- Only after standalone checks should `HPD482Printer` replace `PrinterStub` in
  `src/main.cpp`.

## 4. Physical Timer Hook

- Use the blue timer alarm signal through a 10k/10k divider into an ADC1 GPIO.
- Share ESP32 GND with the timer battery negative terminal.
- Do not use ADC2 while WiFi is enabled.
- Keep `ENABLE_PHYSICAL_HOOK` disabled for calibration.
- Run serial `calib`, trigger the physical alarm, and record:
  - idle ADC value
  - idle p2p
  - alarm ADC range
  - alarm p2p
- Set `HOOK_ADC_PIN` and `ALARM_P2P_THRESHOLD` only after calibration.
- Enable `ENABLE_PHYSICAL_HOOK` and verify one simple slip per alarm episode.
