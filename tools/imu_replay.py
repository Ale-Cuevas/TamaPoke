#!/usr/bin/env python3
"""Replays recorded accelerometer captures through prototype detectors.

The captures in test/data/imu/ were recorded on an AMOLED-2.16 with the
bring-up firmware's IMU probe (accel +-4 g, 62.5 Hz, low-pass filter off),
as t_ms,ax_mg,ay_mg,az_mg,steps. This is the prototype behind the numbers in
docs/FEATURES.md ("Measured on the board"): a software step counter and a
shake detector, ported to C++ in steps.h / motion.h (v1.20). test/test_steps.cpp
checks the port gives the same counts on these captures.

    python3 tools/imu_replay.py test/data/imu/*.csv

Step counter: |a|, minus a slow moving average (gravity), smoothed; a step is
a peak above PEAK_G at least MIN_GAP_S after the previous one, and counting
starts after ENTRY steps in a row, as the QMI8658's own pedometer does.
Shake: on any one axis (gravity removed), SHAKE_REV direction reversals beyond
SHAKE_G, each within SHAKE_GAP_S of the previous one. A shake pauses the step
counter for a second.
"""
import math
import sys

PEAK_G = 0.12  # smoothed dynamic |a| a step must exceed
MIN_GAP_S = 0.28  # fastest step, about 3.5 steps/s
MAX_GAP_S = 2.0  # a slower step breaks the streak
ENTRY = 4  # steps in a row before they count
SLOW_TAU_S = 1.0  # gravity estimate for |a|
FAST_TAU_S = 0.06  # smoothing of the step signal
SHAKE_G = 1.0  # per-axis swing beyond gravity
SHAKE_GAP_S = 0.17  # reversals closer than this are "fast" (about 3 Hz)
SHAKE_REV = 5  # fast reversals on one axis within SHAKE_WIN_S
SHAKE_WIN_S = 1.2
AXIS_TAU_S = 0.4  # per-axis gravity estimate for the shake detector
SHAKE_HOLD_S = 1.0  # steps paused after a shake


def load(path):
    """t_ms,ax_mg,ay_mg,az_mg[,steps] rows as (s, g, g, g). Like test/imu_data.h,
    any line that isn't a sample is skipped: the header, the game's closing
    `[imulog] end`, log lines copied along from a serial monitor."""
    samples = []
    with open(path) as f:
        for line in f:
            try:
                t, x, y, z = (int(v) for v in line.split(",")[:4])
            except ValueError:
                continue
            samples.append((t / 1000, x / 1000, y / 1000, z / 1000))
    return samples


def replay(samples):
    t0, x0, y0, z0 = samples[0]
    slow = fast = None
    grav = [x0, y0, z0]
    last_t = t0
    steps = 0
    streak = []
    prev = prev2 = 0.0
    last_peak = -1e9
    reversals = [[], [], []]
    sign = [0, 0, 0]
    shakes = 0
    shake_until = -1e9
    for t, x, y, z in samples:
        dt = max(t - last_t, 0.001)
        last_t = t
        mag = math.sqrt(x * x + y * y + z * z)
        slow = mag if slow is None else slow + (1 - math.exp(-dt / SLOW_TAU_S)) * (mag - slow)
        dyn = mag - slow
        fast = dyn if fast is None else fast + (1 - math.exp(-dt / FAST_TAU_S)) * (dyn - fast)

        k = 1 - math.exp(-dt / AXIS_TAU_S)
        for i, v in enumerate((x, y, z)):
            grav[i] += k * (v - grav[i])
            hv = v - grav[i]
            s = 1 if hv > SHAKE_G else (-1 if hv < -SHAKE_G else 0)
            if s and s != sign[i]:
                if sign[i]:
                    rev = reversals[i]
                    if rev and t - rev[-1] > SHAKE_GAP_S:
                        rev.clear()
                    rev.append(t)
                sign[i] = s
            reversals[i] = [r for r in reversals[i] if t - r <= SHAKE_WIN_S]
        if max(len(r) for r in reversals) >= SHAKE_REV and t > shake_until:
            shakes += 1
            shake_until = t + SHAKE_HOLD_S
            reversals = [[], [], []]

        if prev > prev2 and prev >= fast and prev > PEAK_G and t - last_peak >= MIN_GAP_S:
            last_peak = t
            if t <= shake_until:
                streak = []
            else:
                if streak and t - streak[-1] > MAX_GAP_S:
                    streak = []
                streak.append(t)
                if len(streak) == ENTRY:
                    steps += ENTRY
                elif len(streak) > ENTRY:
                    steps += 1
        prev2, prev = prev, fast
    return steps, shakes


def main(paths):
    for path in paths:
        steps, shakes = replay(load(path))
        print(f"{path.split('/')[-1]:24s} steps {steps:3d}  shakes {shakes}")


if __name__ == "__main__":
    main(sys.argv[1:])
