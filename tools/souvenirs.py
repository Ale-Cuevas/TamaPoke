#!/usr/bin/env python3
"""Iconos 16x16 de los 25 recuerdos de los viajes (v2.0, Y5): 5 por mapa, en el orden de los ids
(mapa - 1) * 5 + i. Mismo formato que los iconos de objeto (ITEM_ICON): filas de caracteres con la
paleta de tools/sprites.py (spriteColor del firmware), asi que no hace falta tocar species.h.

  uv run --no-project --with pillow tools/souvenirs.py png <fichero.png>   # valida y dibuja a escala 6
  uv run --no-project --with pillow tools/souvenirs.py emit                # regenera souvenir_icons.h
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sprites import G, PALETTE  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def blade(g, x0, y0, x1, y1, half, body, shade, spine, bar=None):
    """Una pluma u hoja: un huso de x0,y0 a x1,y1 con ancho maximo `half`, el raquis y, si se pide, barras."""
    dx, dy = x1 - x0, y1 - y0
    ln = math.hypot(dx, dy)
    ux, uy = dx / ln, dy / ln
    for y in range(16):
        for x in range(16):
            rx, ry = x - x0, y - y0
            t = (rx * ux + ry * uy) / ln
            if t < 0 or t > 1:
                continue
            d = rx * -uy + ry * ux  # distancia al eje, con signo
            w = half * math.sin(math.pi * min(1, t * 0.95 + 0.04))
            if abs(d) <= w:
                c = shade if d > 0.35 * w else body
                if bar and int(t * ln) % 3 == 0 and abs(d) > 0.8:
                    c = bar
                g.px(x, y, c)
    for k in range(int(ln) + 2):  # el raquis
        g.px(int(round(x0 + ux * k * 1.02)), int(round(y0 + uy * k * 1.02)), spine)


def clover():
    g = G(16)
    for cx, cy in ((5, 5), (10, 5), (5, 10), (10, 10)):
        g.disk(cx, cy, 2.9, 2.9, 'g', 'G')
    g.rect(7, 7, 8, 8, 'g')
    g.outline()
    g.rect(7, 2, 8, 12, 'G'); g.rect(2, 7, 12, 8, 'G')
    g.px(7, 7, 'k'); g.px(8, 8, 'k'); g.px(7, 8, 'k'); g.px(8, 7, 'k')
    for x, y in ((9, 9), (10, 11), (11, 12), (11, 13), (12, 14)):
        g.px(x, y, 'k')
    g.px(4, 3, 'w'); g.px(9, 3, 'w')
    return g.rows()


def daisy():
    g = G(16)
    ring = [(8, 2), (12, 4), (13, 8), (12, 12), (8, 13), (4, 12), (3, 8), (4, 4)]
    for x, y in ring:
        g.px(x, y, 'g')
    for x, y in ((6, 2), (10, 2), (11, 3), (5, 3), (13, 6), (13, 10), (12, 11), (11, 13), (10, 14), (6, 14),
                 (5, 13), (4, 11), (3, 10), (3, 6), (4, 5), (12, 5)):
        g.px(x, y, 'G')
    for (cx, cy) in ((8, 2), (13, 8), (3, 8), (8, 13), (12, 12), (4, 4)):
        for dx, dy in ((0, -1), (0, 1), (-1, 0), (1, 0)):
            g.px(cx + dx, cy + dy, 'w')
        g.px(cx, cy, 'f')
    return g.rows()


def feather():
    g = G(16)
    blade(g, 2, 14, 13, 2, 3.4, 'b', 'B', 'N')
    g.outline()
    g.px(1, 15, 'k'); g.px(1, 14, 'k')
    return g.rows()


def honey():
    g = G(16)
    g.rect(3, 5, 12, 14, 'f'); g.rect(11, 5, 12, 14, 'Y')
    g.rect(3, 12, 12, 14, 'o'); g.rect(11, 12, 12, 14, 'O')
    g.rect(4, 2, 11, 4, 'c'); g.rect(10, 2, 11, 4, 'C')
    g.outline()
    g.rect(5, 7, 10, 10, 'y'); g.px(5, 7, 'w'); g.px(6, 7, 'w'); g.px(5, 8, 'w')
    g.rect(5, 9, 10, 10, 'Y')
    g.px(4, 6, 'w'); g.px(4, 7, 'w')
    return g.rows()


def acorn():
    g = G(16)
    g.disk(7.5, 9.5, 4.8, 5.4, 'f', 'Y')
    g.disk(7.5, 4.5, 6, 3, 'V', 'c')
    g.rect(7, 0, 8, 2, 'c')
    g.outline()
    for x in (4, 6, 8, 10):
        g.px(x, 5, 'C')
    for x in (5, 7, 9, 11):
        g.px(x, 4, 'C')
    g.px(5, 9, 'w'); g.px(5, 10, 'w'); g.px(6, 8, 'w')
    return g.rows()


def pebble():
    g = G(16)
    g.disk(7.5, 9, 5.8, 4.2, 's', 'S')
    g.outline()
    g.px(4, 8, 'w'); g.px(5, 7, 'w'); g.px(6, 7, 'w'); g.px(4, 9, 'w')
    g.px(10, 11, 'S'); g.px(9, 12, 'S'); g.px(9, 8, 'S')
    return g.rows()


def crystal(g, x0, x1, ytop, ybot, tip):
    """Un prisma con la punta arriba: borde claro a la izquierda y sombra a la derecha."""
    for y in range(ytop, ybot + 1):
        inset = max(0, tip - (y - ytop))
        for x in range(x0 + inset, x1 - inset + 1):
            rel = x - x0 - inset
            wid = x1 - x0 - 2 * inset
            g.px(x, y, 'w' if rel <= wid // 3 else ('L' if rel >= wid - wid // 3 and wid > 1 else 'l'))


def quartz():
    g = G(16)
    crystal(g, 1, 5, 6, 14, 2)
    crystal(g, 10, 14, 5, 14, 2)
    crystal(g, 4, 11, 1, 14, 3)
    g.outline()
    return g.rows()


def fossil():
    g = G(16)
    g.disk(7.5, 7.5, 6.2, 6.2, 'y', 'Y')
    g.rect(1, 13, 14, 15, 'S'); g.rect(2, 12, 13, 12, 's')
    g.outline()
    sp = [(8, 7), (9, 7), (9, 8), (8, 9), (7, 9), (6, 8), (6, 6), (7, 5), (9, 5), (10, 6), (11, 7), (11, 9), (10, 10),
          (8, 11), (6, 11), (4, 10), (3, 8), (3, 6)]
    for x, y in sp:
        g.px(x, y, 'C')
    g.px(8, 8, 'c')
    g.px(5, 3, 'w'); g.px(6, 3, 'w')
    return g.rows()


def ore():
    g = G(16)
    g.disk(7.5, 8.5, 6.2, 5.6, 'S', 's')
    g.rect(2, 11, 13, 13, 'S')
    g.px(2, 11, '.'); g.px(13, 11, '.')
    g.outline()
    for x, y in ((5, 6), (6, 6), (5, 7), (9, 8), (10, 8), (10, 9), (7, 11), (8, 11), (4, 10), (11, 5)):
        g.px(x, y, 'o')
    for x, y in ((6, 7), (9, 9), (7, 12)):
        g.px(x, y, 'O')
    g.px(5, 5, 'w'); g.px(4, 6, 'w')
    return g.rows()


def moonstone():
    g = G(16)
    g.disk(7.5, 7.5, 6.5, 6.5, 'l', 'L')
    g.outline()
    for y in range(16):
        for x in range(16):
            a = (x - 7.5) ** 2 + (y - 7.5) ** 2
            b = (x - 9.2) ** 2 + (y - 6.8) ** 2
            if a <= 31 and b > 22:
                g.px(x, y, 'w')
    g.px(10, 4, 'w'); g.px(11, 5, 'w')
    return g.rows()


def pinecone():
    g = G(16)
    g.disk(8, 9, 4.6, 6.2, 'c', 'C')
    g.rect(7, 1, 8, 3, 'C')
    g.outline()
    for r, y in enumerate(range(5, 14, 2)):
        for x in range(4 + (r % 2), 12, 2):
            g.px(x, y, 'V')
            g.px(x, y + 1, 'C')
    g.px(6, 5, 'y'); g.px(5, 7, 'y')
    return g.rows()


def mushroom():
    g = G(16)
    g.rect(6, 8, 9, 14, 'y'); g.rect(8, 8, 9, 14, 'Y')
    g.disk(7.5, 6.5, 6.5, 4.8, 'r', 'R')
    g.rect(1, 7, 14, 8, 'r'); g.rect(13, 7, 14, 8, 'R')
    g.outline()
    g.rect(6, 9, 6, 13, 'w')
    for x, y in ((5, 3), (6, 3), (9, 4), (10, 4), (3, 6), (12, 6), (7, 6)):
        g.px(x, y, 'w')
    return g.rows()


def owlfeather():
    g = G(16)
    blade(g, 2, 14, 13, 2, 3.6, 'c', 'C', 'k', bar='y')
    g.outline()
    g.px(1, 15, 'k')
    return g.rows()


def amber():
    g = G(16)
    g.disk(7.5, 9.5, 4.8, 5, 'o', 'O')
    for y in range(3, 7):
        g.rect(7 - (y - 3) // 2, y, 8 + (y - 3) // 2, y, 'o')
    g.px(7, 2, 'o'); g.px(8, 2, 'o')
    g.outline()
    g.rect(5, 8, 5, 11, 'f'); g.px(6, 7, 'f'); g.px(6, 6, 'f'); g.px(5, 7, 'w'); g.px(5, 8, 'w')
    g.px(9, 12, 'f'); g.px(10, 11, 'f')
    g.px(7, 11, 'k'); g.px(8, 10, 'k'); g.px(8, 11, 'k')
    return g.rows()


def ancientleaf():
    g = G(16)
    blade(g, 2, 14, 13, 2, 4.8, 'g', 'G', 'd')
    g.outline()
    for t in range(3, 11, 2):
        x, y = 2 + t * 1.0, 14 - t * 1.1
        g.px(int(x) + 1, int(y) - 1, 'd')
        g.px(int(x) - 1, int(y) + 1, 'd')
    g.px(1, 15, 'k'); g.px(1, 14, 'c'); g.px(2, 15, 'c')
    return g.rows()


def seashell():
    g = G(16)
    for y in range(16):
        for x in range(16):
            dx, dy = x - 7.5, y - 13.0
            r = math.hypot(dx, dy)
            if dy <= 0.5 and r <= 11 * (0.45 + 0.55 * min(1, y / 6.0)) and r <= 12:
                ang = math.degrees(math.atan2(dx, -dy)) if dy < 0 else 90 * (1 if dx > 0 else -1)
                g.px(x, y, 'p' if int(round(ang / 14)) % 2 == 0 else 'P')
    g.rect(5, 12, 10, 14, 'y'); g.rect(9, 12, 10, 14, 'Y')
    g.px(5, 14, '.'); g.px(10, 14, '.')
    g.outline()
    return g.rows()


def starfish():
    g = G(16)
    for y in range(16):
        for x in range(16):
            ang = math.atan2(y - 8.7, x - 7.5)
            r = math.hypot(x - 7.5, y - 8.7)
            k = (ang + math.pi / 2) % (2 * math.pi / 5)
            f = abs(k - math.pi / 5) / (math.pi / 5)
            lim = 6.8 - 4.3 * f
            if r <= lim:
                g.px(x, y, 'o' if (x - 7.5) + (y - 8.7) < 2.5 else 'O')
    g.outline()
    for x, y in ((8, 4), (8, 6), (5, 7), (11, 7), (6, 11), (10, 11), (8, 8)):
        g.px(x, y, 'f')
    return g.rows()


def seaglass():
    g = G(16)
    g.disk(7.5, 8, 6.3, 5.2, 't', 'T')
    g.px(3, 4, 't'); g.px(12, 11, 'T')
    g.outline()
    g.rect(4, 6, 5, 6, 'w'); g.px(4, 7, 'w'); g.px(6, 5, 'w'); g.px(7, 5, 'w')
    g.px(10, 10, 'w'); g.px(9, 11, 'T')
    return g.rows()


def bottle():
    g = G(16)
    g.disk(8, 10, 4.6, 5, 'b', 'B')
    g.rect(6, 3, 9, 8, 'b'); g.rect(9, 3, 9, 8, 'B')
    g.rect(6, 1, 9, 2, 'c')
    g.outline()
    g.rect(7, 6, 8, 12, 'y'); g.px(8, 12, 'Y'); g.px(8, 11, 'Y')
    g.px(7, 8, 'Y'); g.px(7, 10, 'Y')
    g.px(5, 6, 'w'); g.px(5, 7, 'w'); g.px(4, 10, 'w'); g.px(4, 11, 'w')
    return g.rows()


def pearl():
    g = G(16)
    for y in range(9, 16):
        for x in range(16):
            dx = (x - 7.5) / 7.2
            dy = (y - 9) / 6.2
            if dx * dx + dy * dy <= 1:
                g.px(x, y, 'p' if y < 12 else 'P')
    g.disk(7.5, 7.0, 4.4, 4.4, 'w', 's')
    g.outline()
    g.px(6, 5, 'w'); g.px(5, 6, 'w'); g.px(6, 6, 'w')
    g.rect(2, 11, 13, 11, 'y'); g.rect(4, 13, 11, 13, 'p'); g.rect(6, 14, 9, 14, 'P')
    return g.rows()


def oldcoin():
    g = G(16)
    g.disk(7.5, 7.5, 6.6, 6.6, 'f', 'Y')
    g.outline()
    for a in range(0, 360, 30):
        x = 7.5 + 4.9 * math.cos(math.radians(a))
        y = 7.5 + 4.9 * math.sin(math.radians(a))
        g.px(int(round(x)), int(round(y)), 'Y')
    g.rect(7, 4, 8, 11, 'V')
    g.rect(5, 5, 10, 5, 'V'); g.rect(5, 10, 10, 10, 'V')
    g.px(4, 4, 'w'); g.px(5, 3, 'w'); g.px(3, 5, 'w')
    return g.rows()


def mask():
    g = G(16)
    g.disk(7.5, 7.5, 5.8, 7.2, 'o', 'O')
    g.rect(3, 12, 12, 14, 'o')
    g.outline()
    g.rect(4, 5, 6, 7, 'k'); g.rect(9, 5, 11, 7, 'k')
    g.px(5, 6, 'f'); g.px(10, 6, 'f')
    g.rect(7, 8, 8, 10, 'O')
    g.rect(5, 11, 10, 11, 'k')
    g.rect(6, 12, 9, 12, 'y')
    g.px(5, 3, 'y'); g.px(10, 3, 'y')
    return g.rows()


def candle():
    g = G(16)
    g.rect(4, 7, 11, 14, 'y'); g.rect(10, 7, 11, 14, 'Y')
    g.rect(3, 13, 12, 14, 'Y')
    g.disk(7.5, 2.8, 2.2, 3.0, 'r', 'R')
    g.rect(7, 4, 8, 6, 'S')
    g.outline()
    g.rect(6, 2, 9, 3, 'f'); g.rect(7, 1, 8, 4, 'f'); g.px(7, 2, 'w'); g.px(7, 3, 'w')
    g.rect(5, 8, 5, 12, 'w'); g.px(9, 9, 'w'); g.px(9, 10, 'w')
    return g.rows()


def tablet():
    g = G(16)
    g.rect(2, 2, 13, 14, 's'); g.rect(12, 2, 13, 14, 'S'); g.rect(2, 13, 13, 14, 'S')
    g.px(2, 2, '.'); g.px(13, 2, '.'); g.px(3, 14, '.')
    g.outline()
    g.rect(4, 4, 4, 11, 'b'); g.rect(4, 4, 7, 4, 'b'); g.rect(7, 4, 7, 7, 'b'); g.rect(4, 7, 7, 7, 'b')
    g.rect(9, 5, 11, 5, 'b'); g.px(10, 6, 'b'); g.px(9, 7, 'b'); g.px(10, 8, 'b'); g.px(11, 9, 'b'); g.px(9, 10, 'b')
    g.rect(9, 11, 11, 11, 'b')
    g.px(5, 12, 'S'); g.px(12, 12, 'S')
    return g.rows()


def lantern():
    g = G(16)
    g.rect(6, 0, 9, 1, 'U')
    g.rect(5, 2, 10, 3, 'U')
    g.rect(4, 4, 11, 12, 'l'); g.rect(10, 4, 11, 12, 'L')
    g.rect(3, 13, 12, 14, 'U')
    g.outline()
    g.rect(7, 4, 8, 12, 'U')
    g.disk(7.5, 8.2, 1.9, 2.8, 'u'); g.px(7, 8, 'w'); g.px(7, 7, 'w')
    g.px(5, 5, 'w'); g.px(5, 6, 'w')
    g.rect(6, 13, 9, 13, 'q')
    return g.rows()


ICONS = [
    ("CLOVER", clover), ("DAISY", daisy), ("FEATHER", feather), ("HONEY", honey), ("ACORN", acorn),
    ("PEBBLE", pebble), ("QUARTZ", quartz), ("FOSSIL", fossil), ("ORE", ore), ("MOONSTONE", moonstone),
    ("PINECONE", pinecone), ("MUSHROOM", mushroom), ("OWLFEATHER", owlfeather), ("AMBER", amber), ("LEAF", ancientleaf),
    ("SEASHELL", seashell), ("STARFISH", starfish), ("SEAGLASS", seaglass), ("BOTTLE", bottle), ("PEARL", pearl),
    ("COIN", oldcoin), ("MASK", mask), ("CANDLE", candle), ("TABLET", tablet), ("LANTERN", lantern),
]


def build():
    return [(n, f()) for n, f in ICONS]


def validate(icons):
    ok = len(icons) == 25
    for name, rows in icons:
        if len(rows) != 16:
            print(name, "rows", len(rows)); ok = False
        for i, r in enumerate(rows):
            if len(r) != 16:
                print(name, "row", i, len(r)); ok = False
            bad = set(r) - set(PALETTE) - {'.'}
            if bad:
                print(name, "row", i, "chars", bad); ok = False
    return ok


def render(icons, path, scale=6):
    from PIL import Image, ImageDraw
    cols = 5
    cell = 16 * scale + 12
    img = Image.new('RGB', (cols * cell + 12, 5 * cell + 12), '#14161E')
    d = ImageDraw.Draw(img)
    for i, (name, rows) in enumerate(icons):
        ox, oy = 12 + (i % cols) * cell, 12 + (i // cols) * cell
        d.rectangle([ox - 4, oy - 4, ox + 16 * scale + 3, oy + 16 * scale + 3], fill='#252838')
        for r, row in enumerate(rows):
            for c, ch in enumerate(row):
                if ch in PALETTE:
                    d.rectangle([ox + c * scale, oy + r * scale, ox + (c + 1) * scale - 1, oy + (r + 1) * scale - 1],
                                fill=PALETTE[ch])
    img.save(path)
    print("guardado", path)


def emit(icons, path):
    out = ["#pragma once\n// GENERADO por tools/souvenirs.py - edita alli y ejecuta:\n",
           "//   uv run --no-project --with pillow tools/souvenirs.py emit\n",
           "// Los 25 recuerdos de los viajes (v2.0, Y5), 16x16, en el orden de sus ids; los caracteres son los\n",
           "// de spriteColor (species.h). Mismo formato que los iconos de objeto.\n\n"]
    for n, rows in icons:
        out.append(f"static const char* const SPR_SOUV_{n}[16] = {{\n")
        for r in rows:
            out.append(f'  "{r}",\n')
        out.append("};\n\n")
    out.append("static const char *const *const SOUV_ICON[25] = {\n")
    for i in range(0, 25, 5):
        out.append("  " + ", ".join(f"SPR_SOUV_{n}" for n, _ in icons[i:i + 5]) + ",\n")
    out.append("};\n")
    with open(path, 'w') as fh:
        fh.write(''.join(out))
    print("guardado", path)


if __name__ == '__main__':
    icons = build()
    if not validate(icons):
        sys.exit(1)
    if 'emit' in sys.argv:
        emit(icons, os.path.join(ROOT, 'souvenir_icons.h'))
    if 'png' in sys.argv:
        render(icons, sys.argv[sys.argv.index('png') + 1])
