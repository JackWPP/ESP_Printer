#!/usr/bin/env python3
"""加入设备 AP 后，对 TimePrint HTTP 控制接口做烟测。

本脚本只使用 Python 标准库。默认假设主机已连接到 TimePrint-XXXX
接入点，设备地址为 http://192.168.4.1。
"""

from __future__ import annotations

import argparse
import json
import sys
import time
import urllib.error
import urllib.request


if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")


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
            raise AssertionError(f"预期状态 {expected!r}，实际为 {actual!r}: {status}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base-url", default="http://192.168.4.1")
    parser.add_argument("--minutes", type=int, default=1)
    args = parser.parse_args()

    try:
        request_json(args.base_url, "POST", "/api/cmd", {"cmd": "reset"})
        initial = request_json(args.base_url, "GET", "/api/status")
        print("初始状态:", json.dumps(initial, ensure_ascii=False))

        after_set = request_json(args.base_url, "POST", "/api/cmd", {"cmd": "set", "minutes": args.minutes})
        expected_seconds = args.minutes * 60
        if after_set.get("planned_s") != expected_seconds:
            raise AssertionError(f"预期 planned_s={expected_seconds}，实际为 {after_set}")

        running = request_json(args.base_url, "POST", "/api/cmd", {"cmd": "start"})
        assert_state(running, "running")

        time.sleep(1.2)
        ticked = request_json(args.base_url, "GET", "/api/status")
        if ticked.get("elapsed_s", 0) < 1:
            raise AssertionError(f"启动后预期 elapsed_s >= 1，实际为 {ticked}")

        paused = request_json(args.base_url, "POST", "/api/cmd", {"cmd": "pause"})
        assert_state(paused, "paused")

        resumed = request_json(args.base_url, "POST", "/api/cmd", {"cmd": "resume"})
        assert_state(resumed, "running")

        stopped = request_json(args.base_url, "POST", "/api/cmd", {"cmd": "stop"})
        assert_state(stopped, "idle")

        reset = request_json(args.base_url, "POST", "/api/cmd", {"cmd": "reset"})
        assert_state(reset, "idle")
        if reset.get("planned_s") != 0:
            raise AssertionError(f"重置后预期 planned_s=0，实际为 {reset}")

        print("通过：HTTP 控制烟测完成")
        return 0
    except (urllib.error.URLError, TimeoutError) as exc:
        print(
            "失败：无法访问设备 HTTP API。请加入 TimePrint-XXXX AP，"
            f"或传入 --base-url。详情：{exc}",
            file=sys.stderr,
        )
        return 2
    except Exception as exc:
        print(f"失败：{exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
