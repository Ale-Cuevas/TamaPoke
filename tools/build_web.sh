#!/bin/bash
# Regenera web/firmware/tamapoke.bin (firmware combinado, se graba en 0x0) para
# el instalador web, y web/sprites.pak.
# Uso: bash tools/build_web.sh
#
# Compila con PlatformIO (env tamapoke, versiones fijadas en platformio.ini). La
# plataforma ya fusiona bootloader (0x0), particiones (0x8000), boot_app0
# (0xe000) y la app (0x10000) en firmware.factory.bin, con la misma tabla de
# particiones que el FQBN de arduino-cli (app3M_fat9M_16MB).
set -e
cd "$(dirname "$0")/.."

echo "Compilando..."
pio run -e tamapoke

cp .pio/build/tamapoke/firmware.factory.bin web/firmware/tamapoke.bin
echo "OK -> web/firmware/tamapoke.bin ($(du -h web/firmware/tamapoke.bin | cut -f1))"

echo "Empaquetando sprites..."
python3 tools/pack_bundle.py
