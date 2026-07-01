#!/usr/bin/env python3
"""三态演示：循环 idle→working→attention，驱动 StickS3 mascot 三态动画/颜色 + 提示音。

用法：python3 demo_threestate.py --port /dev/cu.usbmodem14501
握手后每 3s 切一个状态，attention 时发 alert(attention,sound=true) 触发提示音（每轮换 session_id 以触发边沿）。
"""
import argparse
import json
import time

import serial

PROTO = "ccb-serial-v1"
_seq = [0]


def nxt():
    _seq[0] += 1
    return _seq[0]


def frame(type_, **kw):
    kw["type"] = type_
    kw["protocol"] = PROTO
    if type_ in ("hello_ack", "device_snapshot", "session_snapshot", "alert", "config"):
        kw.setdefault("seq", nxt())
    return kw


def send(s, f):
    s.write((json.dumps(f, separators=(",", ":")) + "\n").encode())


def drain(s):
    out = []
    while s.in_waiting:
        line = s.readline()
        if line:
            out.append(line.decode("utf-8", errors="replace").rstrip())
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/cu.usbmodem14501")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--cycles", type=int, default=3)
    ap.add_argument("--dwell", type=float, default=3.0)
    args = ap.parse_args()

    s = serial.Serial(args.port, args.baud, timeout=1)
    print("[demo] waiting for device hello (timeout 6s)...")
    deadline = time.time() + 6
    while time.time() < deadline:
        line = s.readline()
        if line and b'"hello"' in line:
            print("[demo] hello received:", line.decode("utf-8", errors="replace").rstrip())
            break
    else:
        print("[demo] no hello, abort")
        s.close()
        return

    send(s, frame("hello_ack", adapter_version="0.1.0", ok=True))
    time.sleep(0.3)
    for r in drain(s):
        print("[demo]  recv:", r)

    states = [
        ("idle", "green"),
        ("working", "red"),
        ("attention", "yellow"),
    ]

    for cyc in range(args.cycles):
        for gs, color in states:
            ds = frame(
                "device_snapshot",
                global_state=gs,
                color=color,
                counts={
                    "sessions": 2,
                    "working": 1 if gs == "working" else 0,
                    "attention": 1 if gs == "attention" else 0,
                    "error": 0,
                },
                focus_session={
                    "id": "abc",
                    "label": "main",
                    "repo": "~/proj",
                    "cwd": "~/proj/src",
                    "state": gs,
                    "title": gs + " session",
                    "line1": "tool: Edit",
                    "line2": "file: x.cpp",
                    "progress": 42,
                },
                alert=None,
            )
            send(s, ds)
            print("[demo] cycle %d  ->  %-10s (%s)" % (cyc, gs, color))
            if gs == "attention":
                time.sleep(0.2)
                # 每轮换 session_id 触发 AlertEdge 边沿 → 重新播放提示音
                send(s, frame("alert", kind="attention", sound=True, session_id="attn%d" % cyc))
            time.sleep(args.dwell)
            for r in drain(s):
                print("[demo]  recv:", r)

    print("[demo] done")
    s.close()


if __name__ == "__main__":
    main()
