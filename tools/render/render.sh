#!/usr/bin/env bash
# Renders TamaPoke's screens to PNG on the PC, as the AMOLED-2.16 shows them:
# the real sketch drawing code, the real Arduino_GFX canvas and the real round
# frame, compiled for the host against the fakes in fakes/.
#
#   tools/render/render.sh                      every scene + screens.png
#   tools/render/render.sh <out_dir> [scene...] some scenes
#   RENDER_FLAGS=-DTP_ROUND_FRAME=0 tools/render/render.sh /tmp/square
#   RENDER_FLAGS="-fsanitize=address,undefined" tools/render/render.sh /tmp/asan
#
# Needs a C++17 compiler, zlib, and PlatformIO with the tamapoke env's
# libraries installed (it runs `pio pkg install -e tamapoke` if they are not).
# The sprites come from tools/sdcard (or TAMAPOKE_SD_ROOT).
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${1:-$ROOT/.pio/render/screens}
shift || true
BUILD=$ROOT/.pio/render
LIBDEPS=$ROOT/.pio/libdeps/tamapoke
PIO_PYTHON=${PIO_PYTHON:-$HOME/.platformio/penv/bin/python}
CXX=${CXX:-g++}
CC=${CC:-gcc}

if [ ! -d "$LIBDEPS/GFX Library for Arduino/src" ] || [ ! -d "$LIBDEPS/U8g2" ]; then
  (cd "$ROOT" && pio pkg install -e tamapoke)
fi
GFX="$LIBDEPS/GFX Library for Arduino/src"
# library objects are cached per RENDER_FLAGS (a sanitizer build can't link plain objects)
OBJ="$BUILD/obj-$(printf '%s' "${RENDER_FLAGS:-}" | cksum | cut -d' ' -f1)"
mkdir -p "$OBJ" "$OUT"

# 1. the sketch as the C++ file PlatformIO compiles (prototypes added)
"$PIO_PYTHON" "$ROOT/tools/render/ino2cpp.py" "$ROOT/TamaPoke.ino" "$BUILD/TamaPoke.ino.cpp"

# 2. the two CJK fonts, out of U8g2's 40 MB font file
if [ ! -s "$BUILD/cjk_fonts.c" ]; then
  {
    echo '#include <stdint.h>'
    echo '#define U8G2_FONT_SECTION(name)'
    LC_ALL=C awk '/^const uint8_t u8g2_font_unifont_t_(japanese3|korean2)\[/ { f = 1 }
                  f { print }
                  f && /";[[:space:]]*$/ { f = 0 }' "$LIBDEPS/U8g2/src/clib/u8g2_fonts.c"
  } > "$BUILD/cjk_fonts.c"
fi

# 3. compile and link (the library objects are built once)
FLAGS=(-std=gnu++17 -O1 -g -w -I"$ROOT/tools/render/fakes" -I"$GFX" -I"$ROOT"
       "-DTAMAPOKE_DEFAULT_SD_ROOT=\"$ROOT/tools/sdcard\"" ${RENDER_FLAGS:-})
objs=()
for src in "$GFX/Arduino_G.cpp" "$GFX/Arduino_GFX.cpp" "$GFX/Arduino_DataBus.cpp" "$GFX/canvas/Arduino_Canvas.cpp"; do
  obj="$OBJ/$(basename "${src%.cpp}").o"
  [ -s "$obj" ] || "$CXX" "${FLAGS[@]}" -c "$src" -o "$obj"
  objs+=("$obj")
done
[ -s "$OBJ/cjk_fonts.o" ] || "$CC" -c "$BUILD/cjk_fonts.c" -o "$OBJ/cjk_fonts.o"
objs+=("$OBJ/cjk_fonts.o")
"$CXX" "${FLAGS[@]}" "-DTAMAPOKE_SKETCH_CPP=\"$BUILD/TamaPoke.ino.cpp\"" \
  "$ROOT/tools/render/render.cpp" "$ROOT/tools/render/stubs.cpp" \
  "$ROOT/pet.cpp" "$ROOT/i18n.cpp" "$ROOT/sdmon.cpp" "${objs[@]}" -lz -o "$BUILD/render"

# 4. render
"$BUILD/render" "$OUT" "$@"
