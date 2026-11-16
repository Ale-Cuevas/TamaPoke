#!/usr/bin/env python3
"""Envia los sprites empaquetados a la SD de la placa por USB.

  python3 tools/send_sd.py                  # envia tools/sdcard/mons/*.bin
  python3 tools/send_sd.py --port /dev/cu.usbmodem101
  python3 tools/send_sd.py --ls             # lista lo que hay en la SD
  python3 tools/send_sd.py --yard           # envia los fondos del patio (tools/yard/out/NN-*.bin
                                            # -> /yard/NN.bin); antes: tools/pack_yard.py
"""
import argparse
import glob
import os
import sys
import time
import serial

def find_port():
    ports = glob.glob('/dev/cu.usbmodem*')
    if not ports:
        sys.exit("no encuentro la placa (/dev/cu.usbmodem*)")
    return ports[0]

def wait_line(ser, expect, timeout=10):
    end = time.time() + timeout
    while time.time() < end:
        line = ser.readline().decode(errors='replace').strip()
        if not line:
            continue
        print(f"  placa: {line}")
        if line == expect:
            return True
        if line == 'ERR':
            return False
    return False

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--port')
    ap.add_argument('--ls', action='store_true')
    ap.add_argument('--yard', action='store_true', help='envia los fondos del patio en vez de los sprites')
    args = ap.parse_args()

    port = args.port or find_port()
    print(f"puerto {port}")
    ser = serial.Serial(port, 115200, timeout=1)
    time.sleep(1.5)
    ser.reset_input_buffer()

    if args.ls:
        ser.write(b'LS\n')
        wait_line(ser, 'DONE', 5)
        return

    if args.yard:
        # NN-nombre.bin -> yard/NN.bin: la placa busca el fondo por su numero
        files = sorted(glob.glob(os.path.join(os.path.dirname(__file__), 'yard', 'out', '[0-9][0-9]-*.bin')))
        if not files:
            sys.exit("no hay fondos; ejecuta antes tools/pack_yard.py")
        targets = [(p, f"yard/{os.path.basename(p)[:2]}.bin") for p in files]
    else:
        files = sorted(glob.glob(os.path.join(os.path.dirname(__file__), 'sdcard', 'mons', '*.bin')))
        if not files:
            sys.exit("no hay .bin; ejecuta antes tools/pack_pmd.py")
        targets = [(p, f"mons/{os.path.basename(p)}") for p in files]

    for path, name in targets:
        size = os.path.getsize(path)
        print(f"-> {name} ({size/1024:.0f} KB)")
        ser.write(f"PUT {name} {size}\n".encode())
        if not wait_line(ser, 'OK', 5):
            print("   la placa no acepto el PUT, sigo con el siguiente")
            continue
        t0 = time.time()
        ok = True
        with open(path, 'rb') as f:
            while chunk := f.read(2048):
                ser.write(chunk)
                # espera el ack '#' del bloque
                ack = ''
                while ack not in ('#', 'ERR'):
                    ack = ser.readline().decode(errors='replace').strip()
                    if ack == '':
                        ok = False
                        break
                if not ok or ack == 'ERR':
                    ok = False
                    break
        if ok and wait_line(ser, 'DONE', 30):
            kbs = size / 1024 / max(0.01, time.time() - t0)
            print(f"   ok ({kbs:.0f} KB/s)")
        else:
            print("   fallo la transferencia")
    print("listo")

if __name__ == '__main__':
    main()
