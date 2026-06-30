#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Host simulator for the claude_code_buddy firmware project (FW-P1-T06).

This is the host-side simulator used to drive the StickS3 firmware and validate
the `ccb-serial-v1` wire protocol during bring-up / integration testing.

Protocol: ccb-serial-v1
  - Transport: serial line (UART), framed as JSON Lines: one JSON object per
    line, each line terminated by a single '\\n'.
  - Encoding: UTF-8, NO BOM.
  - Frame size: a single frame MUST be <= 1024 bytes (including the trailing
    newline). This script asserts that on every frame it builds.
  - Common fields: every frame carries `type` (string) and `protocol`
    (literal "ccb-serial-v1").

Frames this script SENDS (host -> device):
  hello_ack / device_snapshot / session_snapshot / alert / config / ping

Frames this script RECEIVES & prints (device -> host):
  hello / ack / pong / error  (and any other uplink frame, e.g. button/mute/page)

Modes:
  --dry-run (default): no hardware. Prints, in time order, the JSON Lines it
    *would* send (with explanatory comments), and prints the firmware responses
    it *expects* to receive. Lets you validate frame correctness without a board.

  --port /dev/cu.usbmodemXXXX --baud 115200: real serial connection via
    pyserial. Waits for the device's `hello`, sends `hello_ack`, then drives the
    snapshot/alert/config/ping sequence while a receive loop prints and validates
    incoming frames (ack.seq == sent seq, pong.echo_ts_ms == ping.ts_ms).

Usage examples:
  # No hardware: validate protocol frames
  python3 host_sim.py --dry-run

  # Real board
  python3 host_sim.py --port /dev/cu.usbmodem14101 --baud 115200

  # Tune inter-frame pacing
  python3 host_sim.py --dry-run --interval 0.5

Requires Python 3.7+ standard library. Real-serial mode additionally needs
pyserial (`pip install pyserial`).
"""

import argparse
import json
import sys
import time

# --- Protocol constants ---------------------------------------------------

PROTOCOL = "ccb-serial-v1"
ADAPTER_VERSION = "0.1.0"
MAX_FRAME_BYTES = 1024

# Error codes the firmware may report back (for reference / pretty-printing).
ERROR_CODES = (
    "json_parse_error",
    "missing_required_field",
    "unknown_message_type",
    "frame_too_large",
    "version_mismatch",
    "internal_error",
)


# --- Frame construction ---------------------------------------------------

def build_frame(frame_type, seq, **fields):
    """Build a protocol frame dict.

    Every frame gets `type` and `protocol` injected. `seq` is attached when it
    is not None (ping deliberately omits seq). Extra keyword fields are merged
    on top.
    """
    frame = {"type": frame_type, "protocol": PROTOCOL}
    if seq is not None:
        frame["seq"] = seq
    frame.update(fields)
    return frame


def build_hello_ack(seq):
    return build_frame(
        "hello_ack",
        seq,
        adapter_version=ADAPTER_VERSION,
        ok=True,
    )


def build_device_snapshot(seq):
    return build_frame(
        "device_snapshot",
        seq,
        global_state="working",
        color="red",
        counts={"sessions": 2, "working": 1, "attention": 0, "error": 0},
        focus_session={
            "id": "abc123",
            "label": "main",
            "repo": "~/proj",
            "cwd": "~/proj/src",
            "state": "working",
            "title": "editing main.cpp",
            "line1": "tool: Edit",
            "line2": "file: main.cpp",
            "progress": 42,
        },
        alert=None,
    )


def build_session_snapshot(seq):
    return build_frame(
        "session_snapshot",
        seq,
        session={
            "session_id_short": "abc123",
            "state": "working",
            "color": "red",
            "repo": "~/proj",
            "cwd_label": "src",
            "title": "editing",
            "line1": "tool: Edit",
            "line2": "file: main.cpp",
            "progress": 42,
            "age_sec": 120,
        },
    )


def build_alert(seq):
    return build_frame(
        "alert",
        seq,
        kind="attention",
        sound=True,
        session_id="abc123",
    )


def build_config(seq):
    return build_frame(
        "config",
        seq,
        sound_enabled=True,
        privacy_mode=False,
        brightness=60,
        done_ttl_ms=5000,
    )


def build_ping():
    # ping carries a wall-clock timestamp instead of a seq.
    return build_frame("ping", None, ts_ms=int(time.time() * 1000))


# --- Serialization / framing ---------------------------------------------

def serialize_frame(frame):
    """Serialize a frame to compact JSON Lines bytes (UTF-8, no BOM, trailing \\n).

    Asserts the on-wire frame is < MAX_FRAME_BYTES.
    """
    # Compact form: no spaces after separators, ensure_ascii keeps it ASCII-safe
    # and avoids any BOM/multibyte surprises on the wire.
    line = json.dumps(frame, separators=(",", ":"), ensure_ascii=True)
    raw = (line + "\n").encode("utf-8")
    assert len(raw) < MAX_FRAME_BYTES, (
        "frame too large: %d bytes >= %d limit for %r"
        % (len(raw), MAX_FRAME_BYTES, frame.get("type"))
    )
    return raw


# --- Pretty printing helpers ---------------------------------------------

def _tag(label):
    return "[%-10s]" % label


def print_comment(text):
    print("# " + text)


def print_sent(raw):
    sys.stdout.write(_tag("SEND") + " " + raw.decode("utf-8"))
    sys.stdout.flush()


def print_recv(obj, raw=None):
    if raw is not None:
        sys.stdout.write(_tag("RECV") + " " + raw.decode("utf-8"))
    else:
        sys.stdout.write(_tag("RECV") + " " + json.dumps(obj, separators=(",", ":")) + "\n")
    sys.stdout.flush()


def print_warn(text):
    sys.stdout.write(_tag("WARN") + " " + text + "\n")
    sys.stdout.flush()


def print_info(text):
    sys.stdout.write(_tag("INFO") + " " + text + "\n")
    sys.stdout.flush()


# --- Expected (simulated) device responses for dry-run -------------------

def expected_hello():
    return {
        "type": "hello",
        "protocol": PROTOCOL,
        "device": "m5stick-s3",
        "fw_version": "0.1.0",
        "features": ["lcd", "speaker", "buttons"],
        "muted": False,
    }


def expected_ack(seq):
    return {"type": "ack", "protocol": PROTOCOL, "seq": seq, "uptime_ms": 1234}


def expected_pong(ts_ms):
    return {"type": "pong", "protocol": PROTOCOL, "ts_ms": ts_ms + 5, "echo_ts_ms": ts_ms}


# --- Dry-run mode ---------------------------------------------------------

def run_dry_run(interval):
    """Print the planned send sequence plus the expected device responses.

    No serial port is opened. Useful for validating frame structure.
    """
    print_info("dry-run mode: no serial port opened")
    print_info("protocol=%s adapter=%s max_frame=%dB" % (PROTOCOL, ADAPTER_VERSION, MAX_FRAME_BYTES))
    print_comment("Simulated device boots and sends hello on connect.")
    print()

    # 1) device -> host: hello
    print_comment("device -> host: hello (firmware announces itself)")
    print_recv(expected_hello())
    time.sleep(interval)

    # 2) host -> device: hello_ack
    seq = 1
    print_comment("host -> device: hello_ack (acknowledge the device's hello)")
    print_sent(serialize_frame(build_hello_ack(seq)))
    print_comment("device -> host: ack for seq=%d" % seq)
    print_recv(expected_ack(seq))
    time.sleep(interval)

    # 3) host -> device: device_snapshot
    seq = 2
    print_comment("host -> device: device_snapshot (full global state + focus session)")
    print_sent(serialize_frame(build_device_snapshot(seq)))
    print_comment("device -> host: ack for seq=%d" % seq)
    print_recv(expected_ack(seq))
    time.sleep(interval)

    # 4) host -> device: session_snapshot
    seq = 3
    print_comment("host -> device: session_snapshot (single focus session detail)")
    print_sent(serialize_frame(build_session_snapshot(seq)))
    print_comment("device -> host: ack for seq=%d" % seq)
    print_recv(expected_ack(seq))
    time.sleep(interval)

    # 5) host -> device: alert
    seq = 4
    print_comment("host -> device: alert (attention event for a session)")
    print_sent(serialize_frame(build_alert(seq)))
    print_comment("device -> host: ack for seq=%d" % seq)
    print_recv(expected_ack(seq))
    time.sleep(interval)

    # 6) host -> device: config
    seq = 5
    print_comment("host -> device: config (runtime host-side settings)")
    print_sent(serialize_frame(build_config(seq)))
    print_comment("device -> host: ack for seq=%d" % seq)
    print_recv(expected_ack(seq))
    time.sleep(interval)

    # 7) host -> device: ping (no seq; carries ts_ms)
    ping = build_ping()
    print_comment("host -> device: ping (no seq; timestamp for round-trip measurement)")
    print_sent(serialize_frame(ping))
    print_comment("device -> host: pong echoing ping.ts_ms=%d" % ping["ts_ms"])
    print_recv(expected_pong(ping["ts_ms"]))
    time.sleep(interval)

    print()
    print_info("dry-run complete: all frames within %dB limit, sequence OK" % MAX_FRAME_BYTES)


# --- Real serial mode -----------------------------------------------------

def open_serial(port, baud):
    try:
        import serial  # type: ignore
    except ImportError:
        print_warn("pyserial is not installed. Install it with:  pip install pyserial")
        sys.exit(2)
    return serial.Serial(port, baud, timeout=0.1)


def read_line(ser):
    """Read one \\n-terminated line from the serial port. Return bytes or None."""
    try:
        line = ser.readline()
    except Exception as exc:  # noqa: BLE001 - surface any serial error
        print_warn("serial read error: %s" % exc)
        return None
    if not line:
        return None
    if not line.endswith(b"\n"):
        # Partial line; in practice readline() with timeout returns what it has.
        return None
    return line


def parse_frame(raw):
    """Parse a JSON Lines frame. Return (obj, error_str)."""
    try:
        text = raw.decode("utf-8-sig")  # tolerate a stray BOM, but we never emit one
    except UnicodeDecodeError as exc:
        return None, "utf8 decode error: %s" % exc
    text = text.strip()
    if not text:
        return None, "empty line"
    try:
        obj = json.loads(text)
    except json.JSONDecodeError as exc:
        return None, "json_parse_error: %s" % exc
    return obj, None


def handle_recv(obj, raw, pending):
    """Validate & print one received device frame.

    `pending` is a dict of in-flight expectations, e.g. {"ack_seq": int} or
    {"ping_ts": int}. Returns the same dict (possibly mutated).
    """
    ftype = obj.get("type")
    proto = obj.get("protocol")

    if proto != PROTOCOL:
        print_warn("protocol mismatch on %r frame: %r" % (ftype, proto))

    if ftype == "ack":
        seq = obj.get("seq")
        expect = pending.get("ack_seq")
        print_recv(obj, raw)
        if expect is not None and seq != expect:
            print_warn("ack.seq=%r != expected %r" % (seq, expect))
        pending.pop("ack_seq", None)
    elif ftype == "pong":
        echo = obj.get("echo_ts_ms")
        expect = pending.get("ping_ts")
        print_recv(obj, raw)
        if expect is not None and echo != expect:
            print_warn("pong.echo_ts_ms=%r != ping.ts_ms=%r" % (echo, expect))
        pending.pop("ping_ts", None)
    elif ftype == "hello":
        print_recv(obj, raw)
    elif ftype == "error":
        print_recv(obj, raw)
        code = obj.get("code")
        if code not in ERROR_CODES:
            print_warn("unknown error code: %r" % code)
    else:
        # button / mute / page / anything else: print verbatim
        print_recv(obj, raw)
    return pending


def wait_for_hello(ser, timeout_s):
    """Block (up to timeout_s) waiting for the device's hello frame."""
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        raw = read_line(ser)
        if raw is None:
            continue
        obj, err = parse_frame(raw)
        if err:
            print_warn("dropping line (%s): %r" % (err, raw))
            continue
        if obj.get("type") == "hello":
            print_recv(obj, raw)
            return obj
        # Print any frame that arrives before hello (e.g. boot log noise kept as JSON).
        print_recv(obj, raw)
    return None


def send_frame(ser, frame):
    """Serialize and write a frame to the serial port."""
    raw = serialize_frame(frame)
    if ser is not None:
        ser.write(raw)
        ser.flush()
    print_sent(raw)


def run_serial(port, baud, interval, hello_timeout):
    print_info("opening %s @ %d baud" % (port, baud))
    ser = open_serial(port, baud)
    pending = {}

    print_info("waiting for device hello (timeout %.1fs)..." % hello_timeout)
    hello = wait_for_hello(ser, hello_timeout)
    if hello is None:
        print_warn("no hello received within timeout; proceeding anyway")
    else:
        print_info("device hello received: device=%s fw=%s" % (
            hello.get("device"), hello.get("fw_version")))

    # hello_ack
    seq = 1
    send_frame(ser, build_hello_ack(seq))
    pending["ack_seq"] = seq
    drain(ser, pending, interval)

    # device_snapshot
    seq = 2
    send_frame(ser, build_device_snapshot(seq))
    pending["ack_seq"] = seq
    drain(ser, pending, interval)

    # session_snapshot
    seq = 3
    send_frame(ser, build_session_snapshot(seq))
    pending["ack_seq"] = seq
    drain(ser, pending, interval)

    # alert
    seq = 4
    send_frame(ser, build_alert(seq))
    pending["ack_seq"] = seq
    drain(ser, pending, interval)

    # config
    seq = 5
    send_frame(ser, build_config(seq))
    pending["ack_seq"] = seq
    drain(ser, pending, interval)

    # ping
    ping = build_ping()
    send_frame(ser, ping)
    pending["ping_ts"] = ping["ts_ms"]
    drain(ser, pending, interval)

    # final drain
    print_info("final drain of any remaining device frames...")
    drain(ser, pending, interval * 2)
    print_info("done.")
    ser.close()


def drain(ser, pending, dwell):
    """Read and handle device frames for `dwell` seconds."""
    end = time.time() + dwell
    while time.time() < end:
        raw = read_line(ser)
        if raw is None:
            time.sleep(0.02)
            continue
        obj, err = parse_frame(raw)
        if err:
            print_warn("dropping line (%s): %r" % (err, raw))
            continue
        handle_recv(obj, raw, pending)


# --- CLI ------------------------------------------------------------------

def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Host simulator for claude_code_buddy firmware (ccb-serial-v1).",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Do not open a serial port; print the planned send sequence and "
             "expected device responses. Default mode.",
    )
    parser.add_argument(
        "--port",
        default=None,
        help="Serial port device path, e.g. /dev/cu.usbmodem14101. "
             "When given, runs in real-serial mode.",
    )
    parser.add_argument(
        "--baud",
        type=int,
        default=115200,
        help="Baud rate (default 115200).",
    )
    parser.add_argument(
        "--interval",
        type=float,
        default=0.3,
        help="Seconds to wait between sent frames / receive drains (default 0.3).",
    )
    parser.add_argument(
        "--hello-timeout",
        type=float,
        default=5.0,
        help="Seconds to wait for the device hello in serial mode (default 5.0).",
    )
    args = parser.parse_args(argv)

    # Real-serial mode if --port is given; otherwise dry-run (default).
    if args.port:
        run_serial(args.port, args.baud, args.interval, args.hello_timeout)
    else:
        run_dry_run(args.interval)


if __name__ == "__main__":
    main()
