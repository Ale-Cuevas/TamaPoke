#!/usr/bin/env python3
"""Talk to the TamaPoke board over USB serial without restarting it (used by the tamapoke-board-test skill).

Run it with PlatformIO's Python, which has pyserial and esptool:
  PY=~/.platformio/penv/bin/python
  $PY tools/board.py cmd STATS "NEED 0 10"     send commands one by one, print each answer up to DONE
  $PY tools/board.py log 20 [--grep FOOD]      print what the board says for 20 s (taps, FOOD, battles...)
  $PY tools/board.py backup [nvs_backup.bin]   copy the save (NVS 0x9000, 0x5000); restarts the board
  $PY tools/board.py restore nvs_backup.bin    write it back; restarts the board
  $PY tools/board.py reboot                    restart into the game on purpose (to catch the boot log)
  $PY tools/board.py ports                     list the serial ports
Options: --port /dev/ttyACM0 (default: the first /dev/ttyACM* or /dev/cu.usbmodem*), --timeout s.
"""
import glob
import os
import subprocess
import sys
import time

NVS_OFFSET, NVS_SIZE = "0x9000", "0x5000"  # app3M_fat9M_16MB.csv
ESPTOOL = os.path.expanduser("~/.platformio/packages/tool-esptoolpy/esptool.py")


def opt(args, name, default=None):
    if name in args:
        i = args.index(name)
        val = args[i + 1]
        del args[i:i + 2]
        return val
    return default


def find_port(port):
    if port:
        return port
    ports = sorted(glob.glob("/dev/ttyACM*")) + sorted(glob.glob("/dev/cu.usbmodem*"))
    if not ports:
        sys.exit("no board found (/dev/ttyACM*): is it plugged in with a data cable?")
    return ports[0]


def open_port(port):
    try:
        import serial
    except ImportError:
        sys.exit("pyserial missing: run with ~/.platformio/penv/bin/python")
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = port, 115200, 0.2
    # pyserial's defaults drop DTR with RTS high = the USB-Serial/JTAG reset. Set both first.
    s.rts = False
    s.dtr = True
    s.open()
    return s


def read_lines(s, until, timeout, grep=None):
    end, buf = time.time() + timeout, b""
    while time.time() < end:
        buf += s.read(4096)
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            text = line.decode(errors="replace").rstrip("\r")
            if until and text.strip() == until:
                return True
            if grep is None or grep in text:
                print(text, flush=True)
    if buf and grep is None:
        print(buf.decode(errors="replace"), flush=True)
    return False


def cmd_cmd(args, port, timeout):
    if not args:
        sys.exit("usage: board.py cmd COMMAND [COMMAND...]")
    s = open_port(port)
    time.sleep(0.3)
    s.reset_input_buffer()
    ok = True
    for c in args:
        print(f">>> {c}", flush=True)
        s.write((c + "\n").encode())
        if not read_lines(s, "DONE", timeout):
            print(f"!!! no DONE within {timeout} s (just plugged in? wait ~8 s and resend)", flush=True)
            ok = False
    s.close()
    return 0 if ok else 1


def cmd_log(args, port, grep):
    secs = float(args[0]) if args else 10
    s = open_port(port)
    read_lines(s, None, secs, grep)
    s.close()
    return 0


def esptool(port, *a):
    if not os.path.isfile(ESPTOOL):
        sys.exit(f"esptool not found at {ESPTOOL}")
    cmd = [sys.executable, ESPTOOL, "--chip", "esp32s3", "--port", port, *a]
    print(" ".join(cmd), flush=True)
    return subprocess.call(cmd)


def cmd_reboot(port):
    s = open_port(port)
    # releasing reset with DTR still high leaves it in "waiting for download": go in this order
    s.dtr = False
    s.rts = True
    time.sleep(0.1)
    s.rts = False
    s.dtr = True
    read_lines(s, None, 6)
    s.close()
    return 0


def main():
    args = sys.argv[1:]
    port = opt(args, "--port")
    timeout = float(opt(args, "--timeout", "8"))
    grep = opt(args, "--grep")
    if not args:
        sys.exit(__doc__)
    sub, rest = args[0], args[1:]
    if sub == "ports":
        print("\n".join(sorted(glob.glob("/dev/ttyACM*") + glob.glob("/dev/cu.usbmodem*"))) or "none")
        return 0
    port = find_port(port)
    if sub == "cmd":
        return cmd_cmd(rest, port, timeout)
    if sub == "log":
        return cmd_log(rest, port, grep)
    if sub == "backup":
        return esptool(port, "read_flash", NVS_OFFSET, NVS_SIZE, rest[0] if rest else "nvs_backup.bin")
    if sub == "restore":
        if not rest or not os.path.isfile(rest[0]):
            sys.exit("usage: board.py restore FILE (an existing backup)")
        return esptool(port, "write_flash", NVS_OFFSET, rest[0])
    if sub == "reboot":
        return cmd_reboot(port)
    sys.exit(__doc__)


if __name__ == "__main__":
    sys.exit(main())
