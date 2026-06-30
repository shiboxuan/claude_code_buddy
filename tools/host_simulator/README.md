# host_simulator

Host-side simulator for the `claude_code_buddy` firmware project (task
**FW-P1-T06**, integration bring-up). It drives the StickS3 firmware over a
serial link and validates the `ccb-serial-v1` wire protocol.

## Protocol

`ccb-serial-v1` — JSON Lines over UART:

- One JSON object per line, each terminated by a single `\n`.
- UTF-8, no BOM.
- Single frame < 1024 bytes.
- Every frame carries `type` (string) and `protocol` (`"ccb-serial-v1"`).

Frames sent (host → device): `hello_ack`, `device_snapshot`,
`session_snapshot`, `alert`, `config`, `ping`.

Frames received (device → host): `hello`, `ack`, `pong`, `error`, plus any
uplink frame (`button` / `mute` / `page` / …) printed verbatim.

## Requirements

- Python 3.7+ (standard library only for dry-run).
- Real-serial mode additionally needs `pyserial`:

  ```sh
  pip install pyserial
  ```

## Usage

### Dry-run (no hardware)

Validates the full send sequence and the expected device responses without
opening any port:

```sh
python3 host_sim.py --dry-run
```

### Real board

```sh
python3 host_sim.py --port /dev/cu.usbmodem14101 --baud 115200
```

The script waits for the device `hello`, sends `hello_ack`, then drives
`device_snapshot` → `session_snapshot` → `alert` → `config` → `ping`, while a
receive loop prints and validates incoming frames:

- `ack.seq` must equal the sent `seq`.
- `pong.echo_ts_ms` must equal the sent `ping.ts_ms`.

Mismatches produce a `WARN` line.

### Options

| Flag | Default | Description |
| --- | --- | --- |
| `--dry-run` | on (when no `--port`) | Print planned frames + expected responses. |
| `--port PATH` | — | Serial port; switches to real-serial mode. |
| `--baud N` | `115200` | Baud rate. |
| `--interval S` | `0.3` | Seconds between sent frames / receive drains. |
| `--hello-timeout S` | `5.0` | Seconds to wait for the device `hello`. |

## Output tags

Lines are tagged for readability:

- `[SEND]` — a frame written to the device (dry-run: planned frame).
- `[RECV]` — a frame received (dry-run: expected device response).
- `[INFO]` — progress / status.
- `[WARN]` — validation mismatch or recoverable error.
- `# …` — explanatory comment (dry-run).
