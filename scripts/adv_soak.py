from __future__ import annotations

import argparse
import json
import time
import urllib.request


def fetch(url: str) -> dict:
    with urllib.request.urlopen(url.rstrip("/") + "/api/status", timeout=3) as response:
        return json.load(response)


def main() -> int:
    parser = argparse.ArgumentParser(description="MAZ Pocket ADV runtime soak")
    parser.add_argument("--url", default="http://mazpocket.local")
    parser.add_argument("--minutes", type=float, default=30)
    parser.add_argument("--interval", type=float, default=2)
    parser.add_argument("--max-heap-loss-kb", type=int, default=24)
    args = parser.parse_args()

    end = time.monotonic() + args.minutes * 60
    first = fetch(args.url)
    samples = 0
    lowest_heap = int(first.get("free_heap", 0))
    highest_host_q = 0
    highest_ws_q = 0
    max_host_ms = 0
    last = first

    print("ADV soak started. Use COMM/CAPTURE/OPS/CONTROL normally while this runs.")
    while time.monotonic() < end:
        last = fetch(args.url)
        samples += 1
        lowest_heap = min(lowest_heap, int(last.get("free_heap", 0)))
        highest_host_q = max(highest_host_q, int(last.get("host_queue_depth", 0)))
        highest_ws_q = max(highest_ws_q, int(last.get("ws_queue_depth", 0)))
        max_host_ms = max(max_host_ms, int(last.get("host_max_latency_ms", 0)))
        if int(last.get("host_result_drops", 0)):
            raise SystemExit("FAIL: Host result queue dropped completions")
        if int(last.get("ws_frame_drops", 0)):
            raise SystemExit("FAIL: COMM audio queue dropped a frame")
        time.sleep(args.interval)

    start_heap = int(first.get("free_heap", 0))
    heap_loss = max(0, start_heap - int(last.get("free_heap", 0)))
    if heap_loss > args.max_heap_loss_kb * 1024:
        raise SystemExit(f"FAIL: free heap fell by {heap_loss / 1024:.1f} KB")

    print(
        "PASS",
        f"samples={samples}",
        f"heap_start={start_heap // 1024}KB",
        f"heap_low={lowest_heap // 1024}KB",
        f"heap_end={int(last.get('free_heap', 0)) // 1024}KB",
        f"host_q_max={highest_host_q}",
        f"ws_q_max={highest_ws_q}",
        f"host_latency_max={max_host_ms}ms",
        f"ws_first_token={int(last.get('ws_first_token_ms', 0))}ms",
        f"ws_total={int(last.get('ws_total_ms', 0))}ms",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
