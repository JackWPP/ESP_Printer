## 孢子串口打印机 HPD482E-32 — ESP32 适配开发手册

基于孢子开发板配套资料整理，面向 ESP32 平台二次开发。固件版本 V4.6.5。

---

### 零、开发环境搭建

#### 0.1 Arduino IDE 方式

在 Arduino IDE 中开发时，通过"开发板管理器"安装 ESP32 核心包（推荐 esp32 by Espressif Systems，版本 2.x 或 3.x 均可）。开发板选择对应型号（如 "ESP32 Dev Module" 或 "ESP32C3 Dev Module"），波特率选择 115200 用于上传。

项目文件结构要求：Arduino 项目文件夹名必须与 .ino 文件名一致。如果项目包含多个文件（如 image.h），将头文件放在与 .ino 同一目录下即可。示例结构：

```
my_printer_project/
  my_printer_project.ino   # 主程序
  image.h                   # 图片数据（如需打印图片）
```

重要提示：项目路径中不要包含中文字符，否则编译或上传可能失败。

#### 0.2 PlatformIO 方式

如果使用 PlatformIO（VSCode 插件），platformio.ini 配置示例：

```ini
[env:esp32]
platform = espressif32
board = esp32dev          ; 标准 ESP32
; board = esp32-c3-devkitm-1  ; ESP32-C3
framework = arduino
monitor_speed = 115200
```

头文件放在 `include/` 目录或 `src/` 目录下。

#### 0.3 启动序列（重要）

ESP32 与打印机的正确启动顺序为：

1. ESP32 上电，在 `setup()` 中初始化串口，然后等待打印机就绪信号。
2. 打印机上电或复位后，内部启动完成，通过串口发送 `READY\r\n`（7 字节）。
3. ESP32 收到 `READY\r\n` 后，方可开始发送 AT 指令和打印数据。

在示例代码中，用 `delay(1000)` 加 `AT` 测试的方式简化了这一过程。如果需要更可靠的实现，应主动监听 `READY` 信号：

```cpp
#define TXD 17
#define RXD 16

bool waitForReady(unsigned long timeout_ms = 5000) {
  unsigned long start = millis();
  const char* target = "READY";
  int match = 0;
  while (millis() - start < timeout_ms) {
    if (Serial2.available()) {
      char c = Serial2.read();
      if (c == target[match]) {
        match++;
        if (match == 5) { // 完整匹配 "READY"
          // 消耗 \r\n
          unsigned long s2 = millis();
          while (millis() - s2 < 100) {
            if (Serial2.available()) Serial2.read();
          }
          return true;
        }
      } else {
        match = 0; // 不匹配，重置（防止前导干扰字节导致失败）
      }
    }
  }
  return false;
}

void setup() {
  Serial2.begin(115200, SERIAL_8N1, RXD, TXD);
  Serial2.setTimeout(2000); // 设置 readBytes 超时，防止无限阻塞

  if (waitForReady()) {
    // 打印机就绪，开始工作
    Serial2.print("AT");
    // ...
  }
}
```

#### 0.4 Serial2.setTimeout()

原始示例中没有设置串口超时，`while(Serial2.available()<7)` 如果打印机没有响应将导致 ESP32 永久死锁。建议在所有项目开头加上 `Serial2.setTimeout(2000)`，这样 `readBytes()` 在 2 秒内读不够字节数就会返回，程序不会卡死。

---

### 一、硬件概览

孢子串口打印机是一款基于 STM32F103C8T6 主控的热敏打印模块，对外提供 TTL 串口接口，通过 AT 指令控制所有打印行为。开发者只需使用 ESP32 的 UART 串口即可完整驱动该模块，无需额外总线或协议芯片。

**核心规格：**

供电要求为 12V/2A（DC 插座或 IN+/GND 焊盘输入），TTL 电平为 3.3V/0V（容忍 5V），有效打印宽度 48mm（384 像素），分辨率 203DPI，数据缓存 17KB，打印材料为 57mm 宽热敏纸或热敏标签纸。物理尺寸 82mm x 78mm x 20mm，四个 M3 螺丝孔（直径 3mm）。

**打印机接口引脚（6pin 排针）：**

打印机对外暴露一个 6pin 排针，丝印标注为 GND、VCC（3V3 OUT）、TXD、RXD、DTR、RTS。实际连接 ESP32 只需要 3 条线：TXD、RXD、GND。注意 VCC（3V3 OUT）是打印机内部 3.3V 电源输出口，不要连接到 ESP32 的供电端，也不要与 ESP32 的 3.3V 对接。

**打印头上的关键部件：**

打印头型号为 HPD482S，包含 384 个加热点，每个点 0.125mm，由 LAT（锁存）、CLK（时钟）、DI（数据输入）、STBA/STBB（加热控制 A/B 两组各 192 点）五个信号线驱动。这些信号在打印机内部由 STM32 处理，ESP32 侧只需串口通信。

---

### 二、ESP32 接线方案

#### 2.1 标准 ESP32（ESP32-WROOM / ESP32-WROVER）

使用 ESP32 的硬件 UART2（Serial2），引脚定义如下：

| 打印机引脚 | ESP32 引脚 | 说明 |
|-----------|-----------|------|
| TXD | GPIO16 (UART2_RX) | 打印机发送 → ESP32 接收 |
| RXD | GPIO17 (UART2_TX) | 打印机接收 ← ESP32 发送 |
| GND | GND | 共地 |

注意 TXD 和 RXD 是交叉连接：打印机的 TXD 接 ESP32 的 RX（GPIO16），打印机的 RXD 接 ESP32 的 TX（GPIO17）。这不是写错了，串口通信必须 TX 对 RX。

串口初始化代码：

```cpp
#define TXD 17
#define RXD 16

Serial2.begin(115200, SERIAL_8N1, RXD, TXD);
```

#### 2.2 ESP32-C3

ESP32-C3 只有两个 UART，使用 UART1（Serial1），引脚定义：

| 打印机引脚 | ESP32-C3 引脚 | 说明 |
|-----------|--------------|------|
| TXD | GPIO1 (UART1_RX) | 打印机发送 → ESP32-C3 接收 |
| RXD | GPIO0 (UART1_TX) | 打印机接收 ← ESP32-C3 发送 |
| GND | GND | 共地 |

串口初始化代码：

```cpp
#define TXD 0
#define RXD 1

Serial1.begin(115200, SERIAL_8N1, RXD, TXD);
```

#### 2.3 接线注意事项

接线应该尽可能短。过长的杜邦线或面包板跳线会像天线一样吸收干扰信号，导致数据错误、打印失败。实际案例中，使用超长接线或通过面包板转接是打印失败的常见原因。建议在测试确认功能正常后，使用尽量短的直连线。

打印机必须独立供电 12V，不要尝试从 ESP32 的 USB 或 5V/3.3V 引脚取电。ESP32 和打印机通过 GND 共地即可。

---

### 三、通信协议基础

#### 3.1 串口参数

波特率 115200（默认），数据位 8，停止位 1，校验位 None（无）。即标准 115200-8N1 配置。

#### 3.2 通信时序与握手

打印机上电或复位后，内部正常启动，会通过串口发送字符串 `READY\r\n`，表示打印机准备就绪，可以接收 AT 指令和打印数据。

所有 AT 指令均以 "AT" 开头。以 "AT" 开头的任何串口数据都将被视为指令。如果需要打印以 "AT" 开头的文本，应在文本最前面加上转义符 `\`，转义符本身不会被打印。

正常参数设置的回显字符串均为 7 个字节，例如 `GO OK\r\n`、`CN OK\r\n`、`PT OK\r\n`。发生错误时会有其他回显，如指令错误返回 `Error AT\r\n`，缺纸返回 `Error:no paper\r\n`。

只有当打印机返回回显字符串，才意味着处理完成，可以发送新的数据或指令。这一点在 ESP32 编程中非常重要：必须在收到回显后才能发送下一条指令或数据。

#### 3.3 回显模式（Echo）

通过 `AT+EC` 指令设置回显模式：

- `AT+EC=0`：不回显 AT 指令结果
- `AT+EC=1`：仅回显 AT 指令设置结果（如 `CN OK\r\n`）
- `AT+EC=2`（默认）：回显 AT 指令结果和打印结果（打印完成返回 `PT OK\r\n`）

建议在 ESP32 程序中设置为 `AT+EC=2`，这样可以通过检测 `PT OK\r\n`（7 字节）来精确判断打印是否完成，而不依赖延时。

#### 3.4 中文编码

打印机支持两种中文编码方式：GBK 和 UTF-8，出厂默认为 UTF-8。通过 `AT+GU` 指令切换：

- `AT+GU=0`：GBK 编码模式
- `AT+GU=1`：UTF-8 编码模式（默认）

Arduino IDE 的串口默认以 UTF-8 发送数据，因此 ESP32 程序中一般使用 UTF-8 模式即可。如果遇到中文打印乱码，可尝试切换编码方式。

---

### 四、AT 指令完整参考

以下是固件 V4.6.5 的全部 AT 指令参考（共 26 个指令条目，部分指令包含设置和查询两种形式，合计 37 种调用方式）。所有指令的参数字符串均通过串口发送，不带换行符（但加 `\r\n` 也可以）。

#### 4.1 基础与系统指令

**AT — 测试指令**
发送 `AT`，正常返回 `OK\r\n`。用于检测通信是否正常。

**AT+VS? — 查询版本号**
返回固件版本号信息，如 `V4.6.5\r\n`。

**AT+RST — 复位**
指令重启打印机，效果同按 RST 按键。打印机重启后返回 `READY\r\n`。

**AT+SV — 保存参数**
保存所有已设置的 AT 指令参数为默认值。请勿频繁使用，会减少机器寿命。成功返回 `SV OK\r\n`。

**AT+FA — 恢复出厂**
恢复所有 AT 参数为出厂默认值。需复位后生效。请勿频繁使用。成功返回 `FA OK\r\n`。

**AT+TP? — 查询温度**
返回打印头温度值（摄氏度），如 `19.25\r\n`。

**AT+RN? — 查询串口接收大小**
返回串口最大接收字节数。

#### 4.2 走纸控制指令

**AT+GO=\<param\> — 进纸**
使电机进纸 param × 0.125mm。参数范围 [1, 800]。例：`AT+GO=80` 进纸 10mm。返回 `GO OK\r\n`。

**AT+BK=\<param\> — 退纸**
使电机退纸 param × 0.125mm。参数范围 [1, 800]。例：`AT+BK=80` 退纸 10mm。返回 `BK OK\r\n`。

**AT+MS=\<param\> / AT+MS? — 电机转速**
设置/查询不打印时电机转速。参数范围 [0, 7200]，数值越大转速越快，过大容易丢步堵转。

**AT+MM=\<param\> / AT+MM? — 最大走纸距离**
单次走纸指令的最大距离限制，单位 0.125mm。参数范围 [1, 65535]，默认 800。警告：过大可能导致电机运行时间过长而损坏。

#### 4.3 打印质量指令

**AT+DP=\<param\> / AT+DP? — 打印深度（整体）**
设置/查询整体打印颜色深度。参数范围 [0, 50]，数值越大颜色越深、速度越慢。警告：过大可能烧毁打印头！

**AT+LD=\<param\> / AT+LD? — 左半部分深度**
设置/查询左半部分打印深度。参数范围 [0, 20]，默认 2。

**AT+RD=\<param\> / AT+RD? — 右半部分深度**
设置/查询右半部分打印深度。参数范围 [0, 20]，默认 0。

#### 4.4 文本格式指令

**AT+CN=\<param\> / AT+CN? — 汉字字号**
设置/查询中文字号。允许值：16, 24, 32, 48, 64, 72, 80, 96, 128, 192, 384。默认 24。字号值错误返回 `Error:CN\r\n`。

**AT+GU=\<param\> / AT+GU? — 汉字编码方式**
0 = GBK，1 = UTF-8。默认 1。

**AT+SL=\<param\> / AT+SL? — 行间距**
设置/查询文本行间距，单位 0.125mm。参数范围 [0, 255]，默认 0。

**AT+PG=\<param\> / AT+PG? — 打印完进纸距离**
打印完成后自动进纸距离，单位 0.125mm。参数范围 [0, 255]。

**AT+EC=\<param\> — 回显模式**
0 = 不回显，1 = 回显指令结果，2 = 回显指令和打印结果。默认 2。值错误返回 `Error:EC\r\n`。

**AT+PC=\<param\> / AT+PC? — 重复打印次数**
设置/查询重复打印次数。参数范围 [1, 255]，默认 1。

#### 4.5 图片与图形指令

**AT+IM=\<param\> — 打印内置图片**
打印内置 46×50 小图片，param 为横向偏移距离（mm），范围 [0, 47]。返回 `IM OK\r\n`。

**AT+LN=\<param\> — 打印虚线**
打印 param 条虚线，范围 [1, 100]。返回 `LN OK\r\n`。

**AT+LS=\<param\> / AT+LS? — 虚线间距**
设置/查询虚线间距（mm），范围 [1, 31]，默认 1。

**AT+OF=\<param\> / AT+OF? — 图片偏移**
设置/查询打印图片时横向偏移距离（mm），范围 [0, 47]，默认 0。

#### 4.6 串口配置指令

**AT+BD=\<param\> — 波特率**
设置串口波特率。允许值：2400, 4800, 9600, 14400, 19200, 38400, 57600, 115200。默认 115200。设置后无返回（因为波特率已变）。值错误返回 `Error:BD\r\n`。

#### 4.7 打印模式指令

**AT+PM=\<param\> — 打印模式切换**
设置打印机的字符解码模式。参数取值：0 = 中文模式（默认，支持中文+英文混合打印），1 = ASCII 码模式（仅能打印英文和 ASCII 字符，此模式下不能打印中文）。设置成功返回 `PM OK\r\n`。

适用场景：当你的 ESP32 程序只需要打印纯英文内容时，可以切换到 ASCII 模式以提高处理效率。打印完英文后如需恢复中文打印，发送 `AT+PM=0` 切回。

---

### 五、ESP32 编程指南

#### 5.1 基础通信模板

以下是 ESP32 与打印机通信的标准初始化模板，所有示例程序都基于此模式：

```cpp
#define TXD 17
#define RXD 16

char buf[10];

void setup() {
  Serial2.begin(115200, SERIAL_8N1, RXD, TXD);
  delay(1000);

  // 1. 发送 AT 测试通信
  Serial2.print("AT");
  delay(500);
  Serial2.readBytes(buf, 4);
  if (buf[0] == 'O' && buf[1] == 'K') {
    // 2. 设置全回显模式
    Serial2.print("AT+EC=2");
    while (Serial2.available() < 7);
    Serial2.readBytes(buf, 7);

    // 3. 在此编写你的打印逻辑
  }
}

void loop() {
}
```

关键点说明：`Serial2.print("AT")` 发送 AT 指令时不需要加 `\r\n`，打印机会自动识别。收到 4 字节 `OK\r\n` 表示通信正常。设置 `AT+EC=2` 后，每次打印完成都会返回 7 字节的 `PT OK\r\n`，用 `while(Serial2.available()<7)` 等待后 `readBytes(buf,7)` 读取清空缓冲区。

#### 5.2 封装等待回显函数

为了避免在每个打印操作后重复编写等待代码，可以封装一个通用函数：

```cpp
#define TXD 17
#define RXD 16

char buf[10];

void setup() {
  Serial2.begin(115200, SERIAL_8N1, RXD, TXD);
  delay(1000);
}

// 发送 AT 指令并等待回显
void sendCommand(const char* cmd) {
  Serial2.print(cmd);
  while (Serial2.available() < 7);
  Serial2.readBytes(buf, 7);
}

// 发送文本并等待打印完成
void printText(const char* text) {
  Serial2.print(text);
  while (Serial2.available() < 7);
  Serial2.readBytes(buf, 7);
}

void loop() {
  // 使用示例
  sendCommand("AT+CN=32");
  printText("你好世界");
  delay(60000); // 等待一分钟后重复
}
```

注意：上面 `sendCommand` 函数假定打印机处于 EC=2 模式，AT 指令回显都是 7 字节。对于查询类指令（带 `?` 的），回显长度不固定，需要根据具体情况处理。

#### 5.3 示例一：打印中英文（最简程序）

```cpp
#define TXD 17
#define RXD 16

void setup() {
  Serial2.begin(115200, SERIAL_8N1, RXD, TXD);
  delay(1000);
  Serial2.print("ESP32 打印中文测试！");
  delay(2000);
}

void loop() {
}
```

这是最简单的程序，使用延时等待打印完成。适合快速验证接线和通信是否正常。

#### 5.4 示例二：打印不同字号的中文

```cpp
#define TXD 17
#define RXD 16

char buf[10];

void setup() {
  Serial2.begin(115200, SERIAL_8N1, RXD, TXD);
  delay(1000);

  Serial2.print("AT");
  delay(500);
  Serial2.readBytes(buf, 4);
  if (buf[0] == 'O' && buf[1] == 'K') {
    Serial2.print("AT+EC=2");
    while (Serial2.available() < 7);
    Serial2.readBytes(buf, 7);

    int sizes[] = {16, 24, 32, 48, 64, 72, 80, 96, 128, 192, 384};
    for (int i = 0; i < 11; i++) {
      char cmd[20];
      sprintf(cmd, "AT+CN=%d", sizes[i]);
      Serial2.print(cmd);
      while (Serial2.available() < 7);
      Serial2.readBytes(buf, 7);

      char text[30];
      sprintf(text, "打印%d号中文测试", sizes[i]);
      Serial2.print(text);
      while (Serial2.available() < 7);
      Serial2.readBytes(buf, 7);
    }
  }
}

void loop() {
}
```

支持的全部字号为：16, 24, 32, 48, 64, 72, 80, 96, 128, 192, 384。字号 384 占据整个打印宽度。

#### 5.5 示例三：打印图片

打印图片需要先使用取模软件将图片转换为二进制数组，然后通过 `Serial2.write()` 逐字节发送。

image.h 文件中的数组格式：

```cpp
const unsigned char Image1[] = {0X10, 0X01, 0X00, 0X2E, 0X00, 0X32, ...};
const unsigned char Image2[] = {0X10, 0X01, 0X01, 0X80, 0X00, 0XF4, ...};
```

**图像头数据格式（前 6 字节）：**

取模软件生成的数组前 6 个字节是图像描述头，由 Image2Lcd 等工具自动添加。格式如下：

| 字节位置 | 含义 | Image1 示例值 | 解读 |
|---------|------|-------------|------|
| 0 | 数据类型标记 | 0x10 | 固定为 0x10（表示 Image2Lcd 格式） |
| 1 | 位深度/扫描方向 | 0x01 | 高位 = 位深度(0=单色,1=4色)，低位 = 扫描方向(0=垂直,1=水平) |
| 2-3 | 图像宽度(像素) | 0x00, 0x2E | 大端序，0x002E = 46 像素 |
| 4-5 | 图像高度(像素) | 0x00, 0x32 | 大端序，0x0032 = 50 像素 |

Image2 的头部为 0x10, 0x01, 0x01, 0x80, 0x00, 0xF4，解读为：单色、水平扫描、宽度 384 像素 (0x0180)、高度 244 像素 (0x00F4)。

第 6 字节起为像素数据，每字节 8 个像素，高位在前（MSB first）。一行像素宽度不是 8 的整数倍时，末尾用 0 填充。

注意：这 6 字节的图像头是 Image2Lcd 格式特有的，打印机固件能识别这个头部。如果你用其他工具生成图像数据（不含头部），需要手动添加或确保打印机能正确解析。

image.h 空白模板（可直接复制使用）：

```cpp
#ifndef __IMAGE_H
#define __IMAGE_H

/*
 * 待打印图片数据 for ESP32
 * 
 * 如何生成图片数据：
 * 1. 使用 Image2Lcd 软件，输出类型选"C语言数组(.c)"，
 *    水平扫描、单色、勾选"包含图像头数据"、勾选"高位在前"
 * 2. 最大宽度设 384，最大高度设 9999（让宽度能等比缩放到 384）
 * 3. 保存后打开 .c 文件，将花括号内的数据复制粘贴到下方数组中
 *
 * 数组格式：const unsigned char 数组名[] = { 图像数据 };
 * 总数据量不要超过 17KB（打印机缓存上限）
 */

// 在此粘贴 Image2Lcd 取模后的图像数据
const unsigned char Image1[] = {
  // 将取模得到的十六进制数据粘贴到这里
};

#endif
```

主程序：

```cpp
#include "image.h"

#define TXD 17
#define RXD 16

char buf[10];
int i;

void setup() {
  Serial2.begin(115200, SERIAL_8N1, RXD, TXD);
  delay(1000);

  Serial2.print("AT");
  delay(500);
  Serial2.readBytes(buf, 4);
  if (buf[0] == 'O' && buf[1] == 'K') {
    Serial2.print("AT+EC=2");
    while (Serial2.available() < 7);
    Serial2.readBytes(buf, 7);

    for (i = 0; i < sizeof(Image1); i++) Serial2.write(Image1[i]);
    while (Serial2.available() < 7);
    Serial2.readBytes(buf, 7);

    for (i = 0; i < sizeof(Image2); i++) Serial2.write(Image2[i]);
    while (Serial2.available() < 7);
    Serial2.readBytes(buf, 7);
  }
}

void loop() {
}
```

注意发送图片数据使用的是 `Serial2.write()`（发送原始字节），而不是 `Serial2.print()`（发送 ASCII 字符串）。这是非常关键的区别。

#### 5.6 示例四：打印文章（长文本）

```cpp
#define TXD 17
#define RXD 16

char buf[10];
char *text = "丞相祠堂何处寻，锦官城外柏森森。"
             "映阶碧草自春色，隔叶黄鹂空好音。"
             "三顾频烦天下计，两朝开济老臣心。"
             "出师未捷身先死，长使英雄泪满襟。";

void setup() {
  Serial2.begin(115200, SERIAL_8N1, RXD, TXD);
  delay(1000);

  Serial2.print("AT");
  delay(500);
  Serial2.readBytes(buf, 4);
  if (buf[0] == 'O' && buf[1] == 'K') {
    Serial2.print("AT+EC=2");
    while (Serial2.available() < 7);
    Serial2.readBytes(buf, 7);

    Serial2.print(text);
    while (Serial2.available() < 7);
    Serial2.readBytes(buf, 7);
  }
}

void loop() {
}
```

打印机内部有 17KB 缓存，只要文本数据不超过缓存容量，可以一次性发送整篇文章，打印机会自动排版换行并打印。

#### 5.7 示例五：综合功能演示

```cpp
#define TXD 17
#define RXD 16

char buf[10];

void setup() {
  Serial2.begin(115200, SERIAL_8N1, RXD, TXD);
  delay(1000);

  Serial2.print("AT");
  delay(500);
  Serial2.readBytes(buf, 4);
  if (buf[0] == 'O' && buf[1] == 'K') {
    Serial2.print("AT+EC=2");
    while (Serial2.available() < 7);
    Serial2.readBytes(buf, 7);

    // 进纸 3mm
    Serial2.print("AT+GO=24");
    while (Serial2.available() < 7);
    Serial2.readBytes(buf, 7);

    // 打印 5 条间距 1mm 的虚线
    Serial2.print("AT+LN=5");
    while (Serial2.available() < 7);
    Serial2.readBytes(buf, 7);

    // 设置虚线间距为 3mm
    Serial2.print("AT+LS=3");
    while (Serial2.available() < 7);
    Serial2.readBytes(buf, 7);

    // 再打印 5 条间距 3mm 的虚线
    Serial2.print("AT+LN=5");
    while (Serial2.available() < 7);
    Serial2.readBytes(buf, 7);

    // 在偏移 0 位置打印内置小图片
    Serial2.print("AT+IM=0");
    while (Serial2.available() < 7);
    Serial2.readBytes(buf, 7);

    // 退纸 6.625mm
    Serial2.print("AT+BK=53");
    while (Serial2.available() < 7);
    Serial2.readBytes(buf, 7);

    // 进纸 3 个像素点补偿空程
    Serial2.print("AT+GO=3");
    while (Serial2.available() < 7);
    Serial2.readBytes(buf, 7);

    // 在偏移 15 位置打印内置图片
    Serial2.print("AT+IM=15");
    while (Serial2.available() < 7);
    Serial2.readBytes(buf, 7);

    // 查询温度
    Serial2.print("AT+TP?");
    delay(500);
    int len = Serial2.readBytesUntil('\n', buf, 9);
    buf[len] = '\0'; // 确保 null 终止

    // 打印温度值（buf 里包含温度数字，如 "19.25"）
    Serial2.print("当前温度为：");
    Serial2.print(buf);
    while (Serial2.available() < 7);
    Serial2.readBytes(buf, 7);

    // 切换到 ASCII 模式
    Serial2.print("AT+PM=1");
    while (Serial2.available() < 7);
    Serial2.readBytes(buf, 7);

    Serial2.print("Hello world.");
    while (Serial2.available() < 7);
    Serial2.readBytes(buf, 7);

    // 切回中文模式
    Serial2.print("AT+PM=0");
    while (Serial2.available() < 7);
    Serial2.readBytes(buf, 7);
  }
}

void loop() {
}
```

这个示例综合演示了进退纸、虚线打印、内置图片定位打印、温度查询、ASCII 模式切换等功能。注意退纸后再进纸时需要多进几个像素点来补偿机械空程误差。

---

### 六、图片取模与打印

#### 6.1 取模工具

资料包提供了两款取模软件：Image2Lcd（Windows）和 Img2Printer（Windows/Mac）。

#### 6.2 使用 Image2Lcd 取模（单片机打印用）

步骤如下：

第一步，打开图片。如果图片格式无法打开，先用其他工具转为 JPG。不要打开很大的图片，建议先将长图横向分割为接近正方形的小图。

第二步，输出数据类型选择 "C语言数组（*.c）"。如果是在电脑串口助手里打印则选"二进制（*.bin）"。

第三步，选择"水平扫描"。

第四步，选择"单色"。只能选单色，打印机不能识别其他色彩模式。

第五步，勾选"包含图像头数据"。

第六步，勾选"高位在前"。上面三个方框不要勾选。

第七步，查看原始图片的像素宽度和高度。

第八步和第九步，设置输出图像的最大像素宽度和高度。这是最容易出错的地方：这里填的是最大宽度和高度，图像会等比缩放，宽或高不能超过设置值。例如把最大高度设成 10，即使宽度设成 384，宽度也不会等比缩放到 384，因为高度被限制到了 10。如果输出图像宽度达不到 384，说明高度设置得太小了。建议先将高度设一个很大的值（如 9999），宽度设 384。

第十步，调节亮度。不要打印很黑的图片，会减少打印机寿命。

第十一步，保存为 .c 文件，里面包含一个数组，将花括号内的图像数据复制到你的 ESP32 程序的 image.h 中。

#### 6.3 使用 Img2Printer 取模

步骤更简单：打开图片 → 输入待打印图像宽度（最大 384，等比缩放）→ 点击"设置图像"→ 点击"取模"预览 → 保存。会同时保存 .bin（电脑串口助手用）和 .txt（单片机用）两份文件。

#### 6.4 替换 image.h 中的图像数据

在 ESP32 的 image.h 文件中，数组的正确格式为：

```cpp
const unsigned char ImageName[] = { /* 取模后的图像数据 */ };
```

数组大小可以不写。将从取模软件获得的图像数据（花括号内的十六进制数值）替换到数组中即可。

#### 6.5 图片打印的限制

打印机内存为 17KB，如果取模后的图片数据超过此容量，图片将打印不完整。解决方案是用修图软件将大图横向分割为多个小图，逐个取模和打印。

图片宽度不要超过 384 像素，超过部分打印不出来，还会浪费内存。

---

### 七、上传程序的注意事项

**上传 ESP32 程序时不要连接打印机。** 下载程序前请先断开打印机与 ESP32 的连接，因为上传过程会使用串口（GPIO16/GPIO17），打印机的存在可能干扰上传。

下载完程序后，先拔掉 ESP32 电源，接好打印机连线，然后给打印机插上 12V 电源并按一下 RST 复位键（指示灯闪烁表示正常运行），最后给 ESP32 接电启动。

如果上传时报错，检查文件路径中是否包含中文字符。Arduino IDE 在中文路径下可能编译或上传失败，将项目文件夹移到纯英文路径下再试。

---

### 八、调试技巧

#### 8.1 使用电脑串口助手验证

如果用 ESP32 打印有问题，可以先将打印机通过 USB 串口模块（CH340）连接到电脑，用串口调试助手（如 XCOM、SSCOM、COM-HC）手动发送 AT 指令验证打印机是否正常。串口参数：115200, 8N1。

也可以反过来，将 ESP32 的串口输出通过串口模块接到电脑上，用串口助手观察 ESP32 发送的数据是否正确。

#### 8.2 打印机状态指示灯

电源指示灯（DC 座旁边红灯）：接通 12V 电源后常亮。

运行指示灯（右下角红灯）：正常运行时闪烁。如果常亮不闪烁，说明缺纸或环境温度过高/过低（应在 15-25°C 环境下使用）。

#### 8.3 按键功能

KEY 按键：长按进纸，短按一下再长按退纸。

RST 按键：复位打印机。打印过程中按此键可紧急停止。

---

### 九、常见问题排查

**打印不出来 / 打印机没反应：**
检查 TXD/RXD 是否交叉连接正确（打印机 TXD → ESP32 RX，打印机 RXD → ESP32 TX）。检查 GND 是否共地。检查打印机运行指示灯是否闪烁（常亮表示缺纸或温度异常）。检查接线是否过长或接触不良。确认波特率设置正确（115200）。

**打印乱码：**
检查中文编码设置（AT+GU=1 为 UTF-8，AT+GU=0 为 GBK）。Arduino IDE 源文件默认 UTF-8 编码，应使用 AT+GU=1。

**图片打印不全：**
图片数据超过 17KB 缓存容量。将大图分割为多个小图分别取模打印。

**图片太小 / 宽度不够：**
取模软件的最大高度设置太小，导致等比缩放后宽度达不到 384。增大最大高度值。

**走纸不准 / 图片歪斜：**
退纸后再进纸时存在机械空程误差，进纸时多进几个像素点补偿（如 `AT+GO=3`）。

---

### 十、硬件内部参考（重开发资料）

以下信息来自重开发资料，虽为 STM32 内部实现细节，但对于理解打印机工作原理和进行更精细的 ESP32 控制有参考价值。

#### 10.1 打印头时序

HPD482S 打印头有 384 个加热点，分为两组（前 192 点和后 192 点），分别由 STBA 和 STBB 控制加热。数据通过 DI 线在 CLK 时钟下串行移入，384 位数据全部移入后通过 LAT 信号锁存，然后 STBA/STBB 控制加热。加热时间（HEAT_DEEP）影响打印深度，默认约 2000-3000 微秒。

#### 10.2 步进电机参数

打印机使用 4 线 2 相步进电机，4 步驱动序列为：STEP1(0,1,0,1) → STEP2(0,1,1,0) → STEP3(1,0,1,0) → STEP4(1,0,0,1)。每 2 步对应 0.125mm 走纸精度。默认步进速度 5ms/步。

#### 10.3 字库结构（W25Q32 SPI Flash）

打印机内部使用 W25Q32（4MB SPI Flash）存储中文字库，包含 8 种字库：ASCII 1206/1608/2412/3216 四种尺寸、UNICODE 字模、GBK 12/16/24 三种汉字尺寸。字库按横向取模方式一排列。这部分由打印机内部 STM32 处理，ESP32 只需发送文本字符串即可。

#### 10.4 温度保护

打印机具备高温保护功能。打印头温度过高时会自动停止打印。可通过 `AT+TP?` 查询实时温度。在 ESP32 程序中建议在长时间连续打印时定期检查温度。

---

### 附录 A：单位换算速查

| 单位 | 换算关系 |
|------|---------|
| 走纸参数 → 毫米 | 参数 × 0.125mm |
| 毫米 → 走纸参数 | 毫米 ÷ 0.125 = 毫米 × 8 |
| 像素点 → 毫米 | 像素点 × 0.125mm（203DPI） |
| 打印宽度 | 384 像素 = 48mm |

常用走纸距离速查：8 = 1mm，24 = 3mm，40 = 5mm，80 = 10mm，160 = 20mm，400 = 50mm。

### 附录 B：AT 指令回显字节数参考

| 场景 | 回显内容 | 字节数 |
|------|---------|--------|
| AT 测试 | OK\r\n | 4 |
| 设置类指令成功 | XX OK\r\n（如 CN OK\r\n） | 7 |
| 打印完成 | PT OK\r\n | 7 |
| 复位后就绪 | READY\r\n | 7 |
| 指令错误 | Error AT\r\n | 10 |
| 缺纸错误 | Error:no paper\r\n | 16 |
| 查询温度 | 如 19.25\r\n | 不定长 |
| 查询版本号 | 如 V4.6.5\r\n | 不定长 |

### 附录 C：可复用驱动类参考（HPD482Printer）

以下是一个封装好的打印机驱动类，coding agent 可以直接在此基础上开发。将以下代码分别保存为 `HPD482Printer.h` 和 `HPD482Printer.cpp`。

**HPD482Printer.h：**

```cpp
#ifndef HPD482_PRINTER_H
#define HPD482_PRINTER_H

#include <Arduino.h>

class HPD482Printer {
public:
  HPD482Printer(HardwareSerial& serial, int rxPin, int txPin, unsigned long baud = 115200);

  bool begin(unsigned long readyTimeout = 5000);
  bool isReady() const { return ready; }

  // AT 指令封装
  bool sendCommand(const char* cmd);         // 发送设置类指令，等待 7 字节回显
  String queryCommand(const char* cmd);      // 发送查询类指令（带?），返回结果字符串（失败返回空）

  // 打印功能
  bool printText(const char* text);          // 打印文本（自动处理 "AT" 开头的转义）
  bool printText(const String& text);        // String 重载
  bool printImage(const unsigned char* data, size_t len);  // 打印取模图片
  bool printDashedLines(int count);          // 打印虚线
  bool setDashedLineSpacing(int spacing);    // 设置虚线间距(mm)
  String queryDashedLineSpacing();           // 查询虚线间距
  bool printBuiltInImage(int offset = 0);    // 打印内置小图片

  // 走纸控制
  bool feedPaper(int steps);                 // 进纸，单位 0.125mm
  bool backFeed(int steps);                  // 退纸，单位 0.125mm
  bool feedPaperMM(float mm);                // 进纸，单位毫米
  bool setMotorSpeed(int speed);             // 电机转速 [0,7200]
  String queryMotorSpeed();
  bool setMaxFeedDistance(int distance);     // 单次最大走纸距离
  String queryMaxFeedDistance();

  // 参数设置
  bool setFontSize(int size);                // 字号：16,24,32,48,64,72,80,96,128,192,384
  String queryFontSize();
  bool setEncoding(bool utf8);               // true=UTF-8, false=GBK
  String queryEncoding();
  bool setLineSpacing(int spacing);           // 行间距，单位 0.125mm
  String queryLineSpacing();
  bool setPrintDepth(int depth);             // 打印深度 [0,50]
  String queryPrintDepth();
  bool setLeftDepth(int depth);              // 左半深度 [0,20]
  String queryLeftDepth();
  bool setRightDepth(int depth);             // 右半深度 [0,20]
  String queryRightDepth();
  bool setPostPrintFeed(int steps);          // 打印完自动进纸距离
  String queryPostPrintFeed();
  bool setEchoMode(int mode);                // 0=不回显, 1=回显指令, 2=全回显
  bool setPrintCount(int count);             // 重复打印次数 [1,255]
  String queryPrintCount();
  bool setASCIIMode(bool ascii);             // true=ASCII模式, false=中文模式
  bool setImageOffset(int offset);           // 图片横向偏移(mm) [0,47]
  String queryImageOffset();
  bool setBaudRate(int baud);                // 设置波特率（自动重配串口）
  String queryMaxReceiveBytes();             // 查询串口最大接收字节数

  // 查询
  float getTemperature();                    // 返回打印头温度（失败返回 -999.0）
  String getVersion();                       // 返回固件版本号

  // 系统
  bool saveSettings();                       // 保存所有参数
  bool factoryReset();                       // 恢复出厂设置
  bool reset();                              // 软件复位（不重配串口）

private:
  HardwareSerial& _serial;
  int _rxPin, _txPin;
  unsigned long _baud;
  bool ready;

  bool waitReady(unsigned long timeout);     // 滑动窗口等待 READY 信号
  bool waitForResponse(int bytes, unsigned long timeout = 5000);
  void flushBuffer();
};

#endif
```

**HPD482Printer.cpp：**

```cpp
#include "HPD482Printer.h"

HPD482Printer::HPD482Printer(HardwareSerial& serial, int rxPin, int txPin, unsigned long baud)
  : _serial(serial), _rxPin(rxPin), _txPin(txPin), _baud(baud), ready(false) {}

bool HPD482Printer::begin(unsigned long readyTimeout) {
  _serial.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
  _serial.setTimeout(2000);
  if (waitReady(readyTimeout)) {
    // 确保 EC=2 模式，这样 printText 后能收到 PT OK 回显
    sendCommand("AT+EC=2");
    return true;
  }
  return false;
}

bool HPD482Printer::waitReady(unsigned long timeout) {
  unsigned long start = millis();
  // 使用滑动窗口匹配 "READY"，防止前导干扰字节导致失败
  const char* target = "READY";
  int match = 0;
  while (millis() - start < timeout) {
    if (_serial.available()) {
      char c = _serial.read();
      if (c == target[match]) {
        match++;
        if (match == 5) { // 匹配到 "READY"
          // 消耗剩余的 \r\n
          unsigned long s2 = millis();
          while (millis() - s2 < 100) {
            if (_serial.available()) _serial.read();
          }
          ready = true;
          return true;
        }
      } else {
        match = 0; // 不匹配，重置
      }
    }
  }
  // 超时后仍尝试 AT 测试作为后备
  _serial.print("AT");
  delay(500);
  char test[4];
  int n = _serial.readBytes(test, 4);
  if (n >= 2 && test[0]=='O' && test[1]=='K') {
    ready = true;
    sendCommand("AT+EC=2");
    return true;
  }
  return false;
}

void HPD482Printer::flushBuffer() {
  while (_serial.available()) _serial.read();
}

bool HPD482Printer::waitForResponse(int bytes, unsigned long timeout) {
  char buf[32];
  unsigned long start = millis();
  int total = 0;
  while (total < bytes && (millis() - start) < timeout) {
    if (_serial.available()) {
      buf[total++] = _serial.read();
    }
  }
  return total >= bytes;
}

String HPD482Printer::queryCommand(const char* cmd) {
  flushBuffer();
  _serial.print(cmd);
  String result = "";
  unsigned long start = millis();
  while (millis() - start < 3000) {
    if (_serial.available()) {
      char c = _serial.read();
      if (c == '\n') break;
      if (c != '\r') result += c;
    }
  }
  // 检查是否返回了错误
  if (result.startsWith("Error")) {
    return "";  // 返回空字符串表示查询失败
  }
  return result;
}

bool HPD482Printer::sendCommand(const char* cmd) {
  flushBuffer();
  _serial.print(cmd);
  return waitForResponse(7);
}

bool HPD482Printer::printText(const char* text) {
  flushBuffer();
  // 处理以 "AT" 开头的文本：自动添加转义符
  if (text[0] == 'A' && text[1] == 'T') {
    _serial.print("\\");
  }
  _serial.print(text);
  return waitForResponse(7); // 等待 "PT OK\r\n"
}

bool HPD482Printer::printText(const String& text) {
  return printText(text.c_str());
}

bool HPD482Printer::printImage(const unsigned char* data, size_t len) {
  flushBuffer();
  for (size_t i = 0; i < len; i++) {
    _serial.write(data[i]);
  }
  return waitForResponse(7);
}

bool HPD482Printer::printDashedLines(int count) {
  char cmd[20];
  sprintf(cmd, "AT+LN=%d", count);
  return sendCommand(cmd);
}

bool HPD482Printer::setDashedLineSpacing(int spacing) {
  char cmd[20];
  sprintf(cmd, "AT+LS=%d", spacing);
  return sendCommand(cmd);
}

String HPD482Printer::queryDashedLineSpacing() {
  return queryCommand("AT+LS?");
}

bool HPD482Printer::printBuiltInImage(int offset) {
  char cmd[20];
  sprintf(cmd, "AT+IM=%d", offset);
  return sendCommand(cmd);
}

bool HPD482Printer::feedPaper(int steps) {
  char cmd[20];
  sprintf(cmd, "AT+GO=%d", steps);
  return sendCommand(cmd);
}

bool HPD482Printer::backFeed(int steps) {
  char cmd[20];
  sprintf(cmd, "AT+BK=%d", steps);
  return sendCommand(cmd);
}

bool HPD482Printer::feedPaperMM(float mm) {
  return feedPaper((int)(mm * 8 + 0.5));
}

bool HPD482Printer::setMotorSpeed(int speed) {
  char cmd[20];
  sprintf(cmd, "AT+MS=%d", speed);
  return sendCommand(cmd);
}

String HPD482Printer::queryMotorSpeed() {
  return queryCommand("AT+MS?");
}

bool HPD482Printer::setMaxFeedDistance(int distance) {
  char cmd[20];
  sprintf(cmd, "AT+MM=%d", distance);
  return sendCommand(cmd);
}

String HPD482Printer::queryMaxFeedDistance() {
  return queryCommand("AT+MM?");
}

bool HPD482Printer::setFontSize(int size) {
  char cmd[20];
  sprintf(cmd, "AT+CN=%d", size);
  return sendCommand(cmd);
}

String HPD482Printer::queryFontSize() {
  return queryCommand("AT+CN?");
}

bool HPD482Printer::setEncoding(bool utf8) {
  return sendCommand(utf8 ? "AT+GU=1" : "AT+GU=0");
}

String HPD482Printer::queryEncoding() {
  return queryCommand("AT+GU?");
}

bool HPD482Printer::setLineSpacing(int spacing) {
  char cmd[20];
  sprintf(cmd, "AT+SL=%d", spacing);
  return sendCommand(cmd);
}

String HPD482Printer::queryLineSpacing() {
  return queryCommand("AT+SL?");
}

bool HPD482Printer::setPrintDepth(int depth) {
  char cmd[20];
  sprintf(cmd, "AT+DP=%d", depth);
  return sendCommand(cmd);
}

String HPD482Printer::queryPrintDepth() {
  return queryCommand("AT+DP?");
}

bool HPD482Printer::setLeftDepth(int depth) {
  char cmd[20];
  sprintf(cmd, "AT+LD=%d", depth);
  return sendCommand(cmd);
}

String HPD482Printer::queryLeftDepth() {
  return queryCommand("AT+LD?");
}

bool HPD482Printer::setRightDepth(int depth) {
  char cmd[20];
  sprintf(cmd, "AT+RD=%d", depth);
  return sendCommand(cmd);
}

String HPD482Printer::queryRightDepth() {
  return queryCommand("AT+RD?");
}

bool HPD482Printer::setPostPrintFeed(int steps) {
  char cmd[20];
  sprintf(cmd, "AT+PG=%d", steps);
  return sendCommand(cmd);
}

String HPD482Printer::queryPostPrintFeed() {
  return queryCommand("AT+PG?");
}

bool HPD482Printer::setEchoMode(int mode) {
  char cmd[20];
  sprintf(cmd, "AT+EC=%d", mode);
  return sendCommand(cmd);
}

bool HPD482Printer::setPrintCount(int count) {
  char cmd[20];
  sprintf(cmd, "AT+PC=%d", count);
  return sendCommand(cmd);
}

String HPD482Printer::queryPrintCount() {
  return queryCommand("AT+PC?");
}

bool HPD482Printer::setASCIIMode(bool ascii) {
  return sendCommand(ascii ? "AT+PM=1" : "AT+PM=0");
}

bool HPD482Printer::setImageOffset(int offset) {
  char cmd[20];
  sprintf(cmd, "AT+OF=%d", offset);
  return sendCommand(cmd);
}

String HPD482Printer::queryImageOffset() {
  return queryCommand("AT+OF?");
}

bool HPD482Printer::setBaudRate(int baud) {
  char cmd[20];
  sprintf(cmd, "AT+BD=%d", baud);
  flushBuffer();
  _serial.print(cmd);
  // 波特率设置后无回显，需要重新初始化串口
  delay(100);
  _serial.end();
  _baud = baud;
  _serial.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
  _serial.setTimeout(2000);
  return true;
}

String HPD482Printer::queryMaxReceiveBytes() {
  return queryCommand("AT+RN?");
}

float HPD482Printer::getTemperature() {
  String result = queryCommand("AT+TP?");
  if (result == "") return -999.0; // 查询失败返回特殊值
  return result.toFloat();
}

String HPD482Printer::getVersion() {
  return queryCommand("AT+VS?");
}

bool HPD482Printer::saveSettings() {
  return sendCommand("AT+SV");
}

bool HPD482Printer::factoryReset() {
  return sendCommand("AT+FA");
}

bool HPD482Printer::reset() {
  flushBuffer();
  _serial.print("AT+RST");
  ready = false;
  // 不调用 begin()，只等待就绪信号
  return waitReady(5000);
}
```

**使用示例：**

```cpp
#include "HPD482Printer.h"

// 标准 ESP32: UART2, GPIO16(RX), GPIO17(TX)
HPD482Printer printer(Serial2, 16, 17);

void setup() {
  if (!printer.begin()) {
    // 打印机未就绪，处理错误
    return;
  }

  // 设置参数
  printer.setFontSize(32);
  printer.setEncoding(true);  // UTF-8

  // 打印文本
  printer.printText("你好，ESP32 打印机！");

  // 打印虚线分隔
  printer.printDashedLines(3);

  // 打印温度
  float temp = printer.getTemperature();
  char buf[50];
  sprintf(buf, "温度: %.1f C", temp);
  printer.printText(buf);

  // 进纸 10mm 让内容完全露出
  printer.feedPaperMM(10);
}

void loop() {
}
```

这个驱动类封装了所有 AT 指令通信细节，包括等待回显、超时保护、缓冲区清理等。coding agent 可以直接 `#include` 后调用高层 API，不需要关心底层串口通信的字节级处理。
