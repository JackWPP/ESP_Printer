# TimePrint Firmware

Production firmware for the ESP32-S3 TimePrint timer/printer device.

## Stack

- PlatformIO + Arduino
- ESP32-S3-N16R8
- C++17
- Native Unity tests for hardware-independent logic
- Embedded vanilla JavaScript/SVG control page served from PROGMEM

## Commands

Serial monitor commands:

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

HTTP/WebSocket command JSON:

```json
{ "cmd": "set", "minutes": 25 }
{ "cmd": "start" }
{ "cmd": "pause" }
{ "cmd": "resume" }
{ "cmd": "stop" }
{ "cmd": "reset" }
{ "cmd": "config", "data": { "ssid": "home_wifi", "pass": "secret" } }
```

Status JSON:

```json
{
  "state": "running",
  "planned_s": 1500,
  "remaining_s": 842,
  "elapsed_s": 658,
  "overtime_s": 0
}
```

## Verification

```bash
pio test -e native
pio run -e esp32s3
pio run -e esp32s3 -t upload --upload-port COM3
pio device monitor -b 115200
```

If `pio` is not available in the shell, load PlatformIO into PATH before using
these commands.

## Hardware Notes

- The default printer is `PrinterStub`; it prints slips to serial so firmware
  behavior can be verified without HPD482 hardware.
- Physical hook is disabled by default. Use `calib` with the blue timer signal
  divided into ADC1 GPIO before enabling `ENABLE_PHYSICAL_HOOK`.
- The real printer driver must target HPD482 AT commands, not ESC/POS.
