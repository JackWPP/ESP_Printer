# TimePrint Project Agent Contract

This file governs the whole repository. Follow it together with any higher
priority system/developer instructions.

## Project Direction

- The production firmware lives under `firmware/`.
- `Explore_Codes/` is reference material only. Do not ship code from it as the
  final implementation without first rebuilding it into the production layout,
  reviewing it, and testing it.
- The firmware stack is PlatformIO + Arduino for ESP32-S3-N16R8, using C++17.
- ESP-IDF paths or local VSCode setup values are developer-machine details; do
  not make them required for the production build.
- The first supported web UI is a firmware-embedded single-page app using
  vanilla JavaScript, inline CSS, and SVG. Do not add npm tooling for the
  firmware UI unless explicitly requested.

## Firmware Architecture

- `lib/TimerCore` is the authoritative timer state machine. It must stay
  hardware-independent and deterministic.
- All control inputs, including serial, HTTP, WebSocket, and future BLE, call
  the same `TimerCore` event API.
- All output adapters implement listener-style boundaries instead of putting
  network, printer, or UI code into `TimerCore`.
- The software timer path and the physical hook path are independent:
  physical hook detection must not call `TimerCore.stop()` and must not infer
  planned or actual timer data.
- ADC work for the physical timer hook must use ADC1 GPIOs only. ADC2 is not
  acceptable once WiFi is enabled.

## Printer Rules

- The printer hardware truth is the HPD482E-32 documentation in `docs/`.
- The real printer driver must use UART 115200, wait for `READY\r\n`, send AT
  commands, and use explicit timeouts. Do not implement it as ESC/POS unless
  the hardware changes.
- The printer requires independent 12V/2A power and common GND with ESP32.
  Never power it from ESP32 3.3V, ESP32 5V, or USB-only rails.
- Until the real printer is integrated, keep `PrinterStub` as the default
  behavior so the firmware remains testable without hardware.

## Git Discipline

- Keep commits small, reviewable, and tied to working milestones.
- Commit after each verified milestone: project scaffold, P0 core/tests,
  WiFi/Web foundation, and later hardware integration.
- Every commit message must use the Lore protocol from the root instructions:
  an intent line first, then useful trailers such as `Tested:` and
  `Not-tested:`.
- Before committing firmware changes, run the strongest available verification:
  at minimum native tests for logic changes and an ESP32-S3 build for firmware
  integration changes.
- Do not commit build artifacts, PlatformIO caches, runtime `.omx/` state,
  serial logs, or local absolute-path IDE settings.

## Verification Baseline

- Native logic: `cd firmware && pio test -e native`
- Firmware build: `cd firmware && pio run -e esp32s3`
- Device upload after port confirmation:
  `cd firmware && pio run -e esp32s3 -t upload --upload-port COM3`
- Serial monitor: `cd firmware && pio device monitor -b 115200`

If `pio` is not available in PATH, record that as a verification gap and avoid
claiming PlatformIO verification passed.
