#!/usr/bin/env bash
# 在 4D Systems GEN4-ESP32 上绕过 auto-reset 失败：手动 DTR/RTS 序列
# 把 ESP32-S3 拉入下载模式，然后调用 esptool write_flash。
#
# 用法：
#   tools/manual_flash.sh
#
# 等价于：按住 BOOT 键 → 按一下 RST → 松开 BOOT → 几秒内跑 esptool。
# 区别是这是用 CH343 的 DTR/RTS 引脚模拟手按，不依赖板子上的 BOOT 按钮。

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FW_BIN="$PROJECT_ROOT/firmware/.pio/build/esp32s3/firmware.bin"
ESPTOOL="$HOME/.platformio/packages/tool-esptoolpy/esptool.py"
VENV_PY="$PROJECT_ROOT/.venv/Scripts/python.exe"

if [ ! -f "$FW_BIN" ]; then
  echo "错误：找不到 $FW_BIN，请先 cd firmware && uv run pio run -e esp32s3" >&2
  exit 1
fi

if [ ! -x "$ESPTOOL" ]; then
  echo "错误：找不到 $ESPTOOL" >&2
  exit 1
fi

# 找到 venv 的 site-packages，给 esptool 提供 pyserial
SITE_PKG="$($VENV_PY -c 'import sys,os; print(os.pathsep.join([p for p in sys.path if "site-packages" in p]))')"
export PYTHONPATH="$SITE_PKG"

# 用 pyserial 模拟一次 BOOT+RST：
#   - 拉低 DTR → EN/CH_PD 复位
#   - 拉高 RTS → GPIO0/BOOT 进入下载模式
#   - 同时释放 → ROM 启动进入 download stub
python <<'PY'
import serial, time
with serial.Serial('COM4', 115200) as ser:
    ser.dtr = False
    ser.rts = True
    time.sleep(0.1)
    ser.dtr = True
    ser.rts = False
    time.sleep(0.05)
    ser.dtr = False
    ser.rts = False
print("[手动复位] 完成，ESP32-S3 应在下载模式")
PY

# 不让 esptool 再触发一次 reset，直接进 flash 写
exec "$ESPTOOL" \
  --chip esp32s3 \
  --port COM4 \
  --baud 115200 \
  --before no_reset \
  --after hard_reset \
  write_flash 0x0 "$FW_BIN"
