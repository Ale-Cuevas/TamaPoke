# Render harness

Draws TamaPoke's screens on the PC exactly as the AMOLED-2.16 shows them, and
saves them as 480x480 PNGs. No board, no flashing.

```bash
tools/render/render.sh                          # every scene -> .pio/render/screens/
tools/render/render.sh /tmp/shots home gallery  # some scenes
RENDER_FLAGS=-DTP_ROUND_FRAME=0 tools/render/render.sh /tmp/square   # without the round frame
RENDER_FLAGS="-fsanitize=address,undefined" tools/render/render.sh /tmp/asan  # drawing code under sanitizers
```

It also writes `screens.png`, the contact sheet used in the README.

## How it works

- `ino2cpp.py` turns `TamaPoke.ino` into C++ with PlatformIO's own `.ino`
  converter, so the PC compiles the same translation unit as the firmware.
- `render.cpp` includes that file and adds a `main()` that puts the game in
  each scene (`scenes()` lists them) and calls the sketch's own `render()`.
- Drawing goes through the real `RoundCanvas` (`round_display.h`) and the real
  Arduino_GFX canvas from the `tamapoke` env's libraries. The "panel" is a
  480x480 canvas (`fakes/Arduino_GFX_Library.h`) that starts full of noise,
  like the panel's RAM at power-on, so a missing black border would show.
- Sprites are read from `tools/sdcard/` (or `TAMAPOKE_SD_ROOT`). The CJK fonts
  are pulled out of U8g2's font file on the first run.
- `fakes/` holds just enough of the Arduino-ESP32 core, Wire, the touch
  driver, SD_MMC and NVS to compile; `stubs.cpp` replaces the RTC/battery and
  audio modules (80 % battery, no USB, the clock the scene sets).

## What it proves, and what it doesn't

It proves layout, colours, text and sprite placement, the canvas offset on the
panel and the round frame, byte for byte: the pixels are the ones the firmware
would send. It doesn't prove anything about the hardware: QSPI timing, touch
mapping, brightness, the panel's own colour response or frame rate. Those are
in `docs/DEVICE_TESTS.md`.

Requirements: g++ (C++17), zlib headers, and PlatformIO (`pio pkg install -e
tamapoke` is run for you if the libraries are missing).
