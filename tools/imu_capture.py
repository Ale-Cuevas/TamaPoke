#!/usr/bin/env python3
"""Records a walk from the running game over USB, for the step counter's tests.

    python3 tools/imu_capture.py walk-pocket-100-1          # 90 s
    python3 tools/imu_capture.py walk-hand-100-1 --secs 120 --port /dev/ttyACM0

Opens the board's serial port without restarting it, sends `IMULOG <s>` and
writes test/data/imu/<name>.csv: t_ms,ax_mg,ay_mg,az_mg,steps, the last column
being the game's own running count. After the countdown, stand still for a
couple of seconds, walk while counting the steps, stop, and stand still until
it ends. It prints how many steps the game counted during the capture; put
the hand count in the file name.
"""
import argparse
import glob
import os
import sys
import time

import serial

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "test", "data", "imu")
HEADER = "t_ms,ax_mg,ay_mg,az_mg,steps"


def find_port():
    ports = sorted(glob.glob("/dev/ttyACM*")) + sorted(glob.glob("/dev/cu.usbmodem*"))
    if not ports:
        sys.exit("board not found (/dev/ttyACM*, /dev/cu.usbmodem*); pass --port")
    return ports[0]


def open_port(port):
    s = serial.Serial()
    s.port = port
    s.baudrate = 115200
    s.timeout = 0.2
    # RTS low and DTR high before opening: pyserial's defaults pulse the
    # ESP32-S3's USB reset and the game (and its step count) restarts
    s.rts = False
    s.dtr = True
    s.open()
    return s


def sample_steps(line):
    """The steps column of a sample line, or None for anything else."""
    parts = line.split(",")
    try:
        [int(v) for v in parts[:4]]
        return int(parts[4]) if len(parts) == 5 else None
    except ValueError:
        return None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("name", help="file name in test/data/imu/, without .csv")
    ap.add_argument("--secs", type=int, default=90, help="capture length (default 90)")
    ap.add_argument("--wait", type=int, default=5, help="countdown before recording (default 5)")
    ap.add_argument("--port")
    ap.add_argument("--force", action="store_true", help="overwrite an existing capture")
    args = ap.parse_args()

    path = os.path.join(OUT_DIR, args.name + ".csv")
    if os.path.exists(path) and not args.force:
        sys.exit(f"{path} exists; pick another name or pass --force")

    s = open_port(args.port or find_port())
    for i in range(args.wait, 0, -1):
        print(f"recording in {i}...", flush=True)
        time.sleep(1)
    s.reset_input_buffer()
    s.write(f"IMULOG {args.secs}\n".encode())
    s.flush()
    print(f"\aRECORDING {args.secs} s: stand still, walk, stop, stand still", flush=True)

    rows, first, last, buf = [], None, None, b""
    start = time.time()
    shown = 0
    while time.time() - start < args.secs + 5:
        buf += s.read(4096)
        *lines, buf = buf.split(b"\n")
        done = False
        for raw in lines:
            line = raw.decode(errors="replace").strip()
            if line == "[imulog] end":
                done = True
                break
            n = sample_steps(line)
            if n is None:
                continue
            rows.append(line)
            first = n if first is None else first
            last = n
        if done:
            break
        el = int(time.time() - start)
        if el >= shown + 10 and first is not None:
            shown = el
            print(f"  {el:3d} s  game counted {last - first}", flush=True)
    s.close()

    if not rows:
        sys.exit("no samples arrived: is the game running, with v1.20 or later?")
    with open(path, "w") as f:
        f.write(HEADER + "\n" + "\n".join(rows) + "\n")
    secs = (int(rows[-1].split(",")[0]) - int(rows[0].split(",")[0])) / 1000
    print(f"\adone: {len(rows)} samples over {secs:.1f} s ({len(rows) / max(secs, 0.001):.1f}/s), "
          f"game counted {last - first} steps -> {os.path.relpath(path, ROOT)}")


if __name__ == "__main__":
    main()
