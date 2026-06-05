# TimePrint Hardware Notes

Use this directory for BOMs, wiring photos, calibration notes, and enclosure
files.

Current hardware assumptions:

- Controller: ESP32-S3-N16R8.
- Printer: HPD482E-32 UART AT-command thermal printer, 12V/2A independent
  supply, shared GND with ESP32.
- Physical timer hook: blue alarm signal wire through a 10k/10k divider into an
  ESP32 ADC1 GPIO. Detect oscillation peak-to-peak, not absolute level.

Record the final COM port, ADC pin, p2p calibration values, and printer wiring
here once verified on the bench.
