#!/usr/bin/env python3
"""Smoke-test TimePrint HTTP control after joining the device AP.

This script uses only the Python standard library. It expects the host to be
connected to the TimePrint-XXXX access point, where the device is reachable at
http://192.168.4.1 by default.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
import urllib.error
import urllib.request


def request_json(base_url: str, method: str, path: str, payload: dict | None = None) -> dict:
    data = None
    headers = {}
    if payload is not None:
        data = json.dumps(payload).encode("utf-8")
        headers["Content-Type"] = "application/json"

    req = urllib.request.Request(
        base_url.rstrip("/") + path,
        data=data,
        headers=headers,
        method=method,
    )

    with urllib.request.urlopen(req, timeout=3) as response:
        raw = response.read().decode("utf-8")
    return json.loads(raw)


def assert_state(status: dict, expected: str) -> None:
    actual = status.get("state")
    if actual != expected:
        raise AssertionError(f"expected state {expected!r}, got {actual!r}: {status}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base-url", default="http://192.168.4.1")
    parser.add_argument("--minutes", type=int, default=1)
    args = parser.parse_args()

    try:
        request_json(args.base_url, "POST", "/api/cmd", {"cmd": "reset"})
        initial = request_json(args.base_url, "GET", "/api/status")
        print("initial:", json.dumps(initial, ensure_ascii=False))

        after_set = request_json(args.base_url, "POST", "/api/cmd", {"cmd": "set", "minutes": args.minutes})
        expected_seconds = args.minutes * 60
        if after_set.get("planned_s") != expected_seconds:
            raise AssertionError(f"expected planned_s={expected_seconds}, got {after_set}")

        running = request_json(args.base_url, "POST", "/api/cmd", {"cmd": "start"})
        assert_state(running, "running")

        time.sleep(1.2)
        ticked = request_json(args.base_url, "GET", "/api/status")
        if ticked.get("elapsed_s", 0) < 1:
            raise AssertionError(f"expected elapsed_s >= 1 after start, got {ticked}")

        paused = request_json(args.base_url, "POST", "/api/cmd", {"cmd": "pause"})
        assert_state(paused, "paused")

        resumed = request_json(args.base_url, "POST", "/api/cmd", {"cmd": "resume"})
        assert_state(resumed, "running")

        stopped = request_json(args.base_url, "POST", "/api/cmd", {"cmd": "stop"})
        assert_state(stopped, "idle")

        reset = request_json(args.base_url, "POST", "/api/cmd", {"cmd": "reset"})
        assert_state(reset, "idle")
        if reset.get("planned_s") != 0:
            raise AssertionError(f"expected planned_s=0 after reset, got {reset}")

        print("PASS: HTTP control smoke test completed")
        return 0
    except (urllib.error.URLError, TimeoutError) as exc:
        print(
            "FAIL: device HTTP API is not reachable. Join the TimePrint-XXXX AP "
            f"or pass --base-url. Details: {exc}",
            file=sys.stderr,
        )
        return 2
    except Exception as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
