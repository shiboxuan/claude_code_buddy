#!/usr/bin/env python3
"""声音测试：发 attention/done/error 三种 alert（未静音），听 3 种音高提示音。

用法：python3 demo_sound.py --port /dev/cu.usbmodem14501
注意：测试期间不要按 B（会切静音抑制声音）。每轮换 session_id 触发 AlertEdge 边沿。
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
    ap.add_argument("--cycles", type=int, default=2)
    args = ap.parse_args()

    s = serial.Serial(args.port, args.baud, timeout=1)
    print("[sound] waiting for hello (check muted field)...")
    muted = None
    deadline = time.time() + 6
    while time.time() < deadline:
        line = s.readline()
        if line and b'"hello"' in line:
            print("[sound] hello:", line.decode("utf-8", errors="replace").rstrip())
            try:
                muted = json.loads(line).get("muted")
            except Exception:
                pass
            break
    if muted:
        print("[sound] !! device is MUTED — press B(short) to unmute first, then retry")
        s.close()
        return

    send(s, frame("hello_ack", adapter_version="0.1.0", ok=True))
    time.sleep(0.3)
    for r in drain(s):
        print("[sound]  recv:", r)

    tones = [("attention", 880, "high"), ("done", 660, "mid"), ("error", 220, "low")]
    for cyc in range(args.cycles):
        for kind, freq, label in tones:
            send(s, frame("alert", kind=kind, sound=True, session_id="%s%d" % (kind, cyc)))
            print("[sound] cycle %d  ->  %-10s (%dHz %s)  LISTEN!" % (cyc, kind, freq, label))
            time.sleep(1.5)
            for r in drain(s):
                print("[sound]  recv:", r)
    print("[sound] done")
    s.close()


if __name__ == "__main__":
    main()
