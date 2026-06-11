#pragma once

// 打印机硬件接线与协议常量。被 main.cpp 与 WebControlAdapter 共享。
// 修改此文件前请确认 4D Systems GEN4-ESP32 板子的实际引脚分配
// （CH343 默认在 GPIO 43/44，HPD482 必须独立到 Serial2）。

#ifndef PRINTER_RX_PIN
#define PRINTER_RX_PIN 16
#endif

#ifndef PRINTER_TX_PIN
#define PRINTER_TX_PIN 17
#endif

#ifndef PRINTER_BAUD
#define PRINTER_BAUD 115200
#endif

#ifndef PRINTER_READY_TIMEOUT_MS
#define PRINTER_READY_TIMEOUT_MS 10000
#endif
