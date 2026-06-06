#!/usr/bin/env python3
"""检查串口上的 TimePrint 启动日志。"""

from __future__ import annotations

import argparse
import sys
import time


if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")


EXPECTED_MARKERS = (
    "TimePrint 固件骨架",
    "命令: set <分钟>",
    "[Web] Web 服务已启动",
    "[Web] 打开 http://",
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM3")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--seconds", type=float, default=8.0)
    parser.add_argument("--no-reset", action="store_true")
    args = parser.parse_args()

    try:
        import serial
    except ImportError:
        print("失败：未安装 pyserial。请先安装 PlatformIO 或 pyserial。", file=sys.stderr)
        return 2

    deadline = time.time() + args.seconds
    captured = ""

    try:
        with serial.Serial(args.port, args.baud, timeout=0.2) as ser:
            if not args.no_reset:
                ser.dtr = False
                ser.rts = True
                time.sleep(0.05)
                ser.rts = False
                time.sleep(0.2)

            while time.time() < deadline:
                chunk = ser.read(256)
                if chunk:
                    captured += chunk.decode("utf-8", errors="replace")
                    if all(marker in captured for marker in EXPECTED_MARKERS):
                        print(captured)
                        print("通过：已找到串口启动标记")
                        return 0
    except Exception as exc:
        print(f"失败：无法读取 {args.port}: {exc}", file=sys.stderr)
        return 2

    print(captured)
    missing = [marker for marker in EXPECTED_MARKERS if marker not in captured]
    print(f"失败：缺少串口启动标记: {missing}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
