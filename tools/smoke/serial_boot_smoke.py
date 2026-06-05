#!/usr/bin/env python3
"""Check TimePrint boot logs on a serial port."""

from __future__ import annotations

import argparse
import sys
import time


EXPECTED_MARKERS = (
    "TimePrint firmware scaffold",
    "commands: set <min>",
    "[Web] server started",
    "[Web] open http://",
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
        print("FAIL: pyserial is not installed. Install PlatformIO or pyserial first.", file=sys.stderr)
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
                        print("PASS: serial boot markers found")
                        return 0
    except Exception as exc:
        print(f"FAIL: could not read {args.port}: {exc}", file=sys.stderr)
        return 2

    print(captured)
    missing = [marker for marker in EXPECTED_MARKERS if marker not in captured]
    print(f"FAIL: missing serial boot markers: {missing}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
