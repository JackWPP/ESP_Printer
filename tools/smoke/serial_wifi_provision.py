#!/usr/bin/env python3
"""Provision TimePrint WiFi credentials over the serial console."""

from __future__ import annotations

import argparse
import re
import sys
import time


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
        print("FAIL: pyserial is not installed. Install PlatformIO or pyserial first.", file=sys.stderr)
        return 2

    command = f"wifiset {args.ssid} {args.password}\n".encode("utf-8")
    captured = ""
    ip_pattern = re.compile(r"STA connected at ([0-9.]+)")
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
                    print(f"PASS: provisioned STA IP {match.group(1)}")
                    return 0
    except Exception as exc:
        print(f"FAIL: could not provision via {args.port}: {exc}", file=sys.stderr)
        return 2

    print_safe(captured)
    print("FAIL: did not observe STA connection before timeout", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
