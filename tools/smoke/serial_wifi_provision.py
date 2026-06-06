#!/usr/bin/env python3
"""通过串口控制台配置 TimePrint WiFi 凭据。"""

from __future__ import annotations

import argparse
import re
import sys
import time


if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")


def print_safe(text: str) -> None:
    sys.stdout.buffer.write(text.encode("utf-8", errors="replace"))
    if not text.endswith("\n"):
        sys.stdout.buffer.write(b"\n")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM3")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--ssid", required=True)
    parser.add_argument("--password", required=True)
    parser.add_argument("--timeout", type=float, default=30.0)
    args = parser.parse_args()

    try:
        import serial
    except ImportError:
        print("失败：未安装 pyserial。请先安装 PlatformIO 或 pyserial。", file=sys.stderr)
        return 2

    command = f"wifiset {args.ssid} {args.password}\n".encode("utf-8")
    captured = ""
    ip_pattern = re.compile(r"STA (?:connected at|已连接，地址) ([0-9.]+)")
    deadline = time.time() + args.timeout

    try:
        with serial.Serial(args.port, args.baud, timeout=0.2) as ser:
            time.sleep(0.5)
            ser.write(command)
            ser.flush()

            while time.time() < deadline:
                chunk = ser.read(256)
                if not chunk:
                    continue
                text = chunk.decode("utf-8", errors="replace")
                captured += text
                match = ip_pattern.search(captured)
                if match:
                    print_safe(captured)
                    print(f"通过：已配置 STA IP {match.group(1)}")
                    return 0
    except Exception as exc:
        print(f"失败：无法通过 {args.port} 配网: {exc}", file=sys.stderr)
        return 2

    print_safe(captured)
    print("失败：超时前未观察到 STA 连接", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
