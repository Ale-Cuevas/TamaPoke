#!/usr/bin/env python3
"""Iconos 16x16 de los 45 recuerdos de los viajes (v2.0, Y5; v3.15: 9 mapas): 5 por mapa, en el orden de los ids
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


# ---------------------------------------------------------------- v3.15: mapas 6-9


def spark(g, x, y, ch='f'):
    """Una chispa en cruz de 5 px con el centro blanco."""
    for dx, dy in ((0, -1), (0, 1), (-1, 0), (1, 0)):
        g.px(x + dx, y + dy, ch)
    g.px(x, y, 'w')


def ember():
    # volcan, comun: un carbon encendido con grietas de fuego y dos chispas
    g = G(16)
    g.disk(7.5, 10, 6, 4.6, 'R', 'Q')
    g.outline()
    for x, y in ((4, 9), (5, 10), (6, 10), (7, 11), (8, 11), (9, 10), (10, 9), (11, 10), (7, 8), (8, 8)):
        g.px(x, y, 'o')
    for x, y in ((6, 10), (7, 11), (9, 10)):
        g.px(x, y, 'f')
    g.px(5, 8, 'r'); g.px(4, 8, 'r'); g.px(10, 12, 'r')
    spark(g, 4, 3)
    g.px(11, 2, 'f'); g.px(12, 4, 'o')
    return g.rows()


def obsidian():
    # volcan, comun: una lasca de cristal volcanico negro-violeta, con caras y un destello
    g = G(16)
    shape = [(7, 1), (8, 1), (11, 4), (13, 8), (12, 12), (9, 14), (5, 14), (2, 11), (3, 6)]
    for y in range(16):
        for x in range(16):
            # punto dentro del poligono (par / impar)
            inside = False
            j = len(shape) - 1
            for i in range(len(shape)):
                xi, yi = shape[i]
                xj, yj = shape[j]
                if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi) + xi:
                    inside = not inside
                j = i
            if inside:
                g.px(x, y, 'I' if x + y > 15 else 'i')
    g.outline()
    for k in range(6):
        g.px(7 + k // 2, 3 + k, 'i')
    g.px(5, 6, 'l'); g.px(5, 7, 'l'); g.px(6, 5, 'w'); g.px(10, 9, 'u')
    return g.rows()


def magmastone():
    # volcan, poco comun: una roca gris oscura con vetas de magma
    g = G(16)
    g.disk(7.5, 8.5, 6.4, 5.8, 'S', 'k')
    g.rect(2, 11, 13, 13, 'S')
    g.px(2, 11, '.'); g.px(13, 11, '.')
    g.outline()
    vein = [(3, 7), (4, 8), (5, 8), (6, 9), (7, 9), (8, 8), (9, 8), (10, 9), (11, 10), (12, 10),
            (8, 4), (8, 5), (7, 6), (7, 7), (7, 8), (6, 11), (5, 12)]
    for x, y in vein:
        g.px(x, y, 'o')
    for x, y in ((7, 9), (8, 8), (7, 7)):
        g.px(x, y, 'f')
    g.px(10, 10, 'r'); g.px(4, 9, 'r')
    g.px(5, 5, 's'); g.px(4, 6, 's')
    return g.rows()


def firefeather():
    # volcan, poco comun: una pluma roja y naranja con el raquis de fuego
    g = G(16)
    blade(g, 2, 14, 13, 2, 3.6, 'o', 'r', 'f')
    g.outline()
    g.px(1, 15, 'k'); g.px(1, 14, 'k')
    g.px(13, 1, 'f'); g.px(14, 3, 'o'); g.px(11, 0, 'o')
    return g.rows()


def phoenixember():
    # volcan, raro: una llama viva, de rojo a blanco en el centro
    g = G(16)
    for y in range(16):
        for x in range(16):
            # gota invertida: ancha abajo, punta arriba
            t = (y - 1) / 14.0
            if t < 0:
                continue
            half = 6.2 * math.sin(math.pi * min(1, t * 0.62 + 0.05)) ** 1.3
            wob = 0.9 * math.sin(y * 1.3)
            if abs(x - 7.5 - wob * (1 - t)) <= half:
                g.px(x, y, 'r')
    g.outline()
    g.disk(7.5, 10.5, 3.8, 4.2, 'o')
    g.disk(7.8, 11.5, 2.3, 2.8, 'f')
    g.disk(7.8, 12.2, 1.0, 1.4, 'w')
    g.px(6, 5, 'o'); g.px(9, 3, 'o'); g.px(9, 4, 'o')
    g.px(2, 2, 'f'); g.px(13, 5, 'f'); g.px(14, 4, 'w')
    return g.rows()


def battery():
    # central, comun: una pila de pie con su borne, la banda amarilla y el "+"
    g = G(16)
    g.rect(6, 0, 9, 2, 's'); g.rect(9, 0, 9, 2, 'S')
    g.rect(4, 2, 11, 15, 'S'); g.rect(10, 2, 11, 15, 'k')
    g.rect(4, 2, 10, 6, 'f'); g.rect(9, 2, 10, 6, 'Y')
    g.outline()
    g.rect(5, 9, 9, 14, 'S'); g.rect(5, 9, 5, 14, 's')
    g.rect(7, 3, 7, 5, 'k'); g.rect(6, 4, 8, 4, 'k')
    g.rect(6, 11, 8, 11, 'w')
    g.rect(5, 3, 5, 5, 'w')
    return g.rows()


def magnet():
    # central, comun: un iman de herradura rojo con las puntas de plata
    g = G(16)
    for y in range(16):
        for x in range(16):
            r = math.hypot(x - 7.5, y - 7.5)
            if y <= 7.5 and 2.6 <= r <= 6.6:
                g.px(x, y, 'r' if x < 8 else 'R')
    g.rect(1, 7, 4, 11, 'r'); g.rect(11, 7, 14, 11, 'R')
    g.rect(1, 12, 4, 14, 's'); g.rect(11, 12, 14, 14, 's')
    g.rect(4, 12, 4, 14, 'S'); g.rect(14, 12, 14, 14, 'S')
    g.outline()
    g.px(3, 5, 'w'); g.px(4, 3, 'w'); g.px(2, 7, 'w')
    g.rect(1, 12, 4, 12, 'k'); g.rect(11, 12, 14, 12, 'k')
    return g.rows()


def coppercoil():
    # central, poco comun: una bobina de hilo de cobre en un nucleo gris
    g = G(16)
    g.rect(1, 6, 14, 9, 'S'); g.rect(1, 6, 14, 6, 's')
    for x0 in range(3, 13, 2):
        g.rect(x0, 3, x0, 12, 'o')
        g.px(x0, 3, 'f'); g.px(x0, 4, 'f')
        g.px(x0, 12, 'O'); g.px(x0, 11, 'O')
    g.outline()
    for x0 in range(4, 13, 2):
        g.rect(x0, 4, x0, 11, 'C')
    g.px(0, 7, 'o'); g.px(15, 8, 'o'); g.px(0, 8, 'O'); g.px(15, 7, 'O')
    return g.rows()


def thunderstone():
    # central, poco comun: la piedra trueno, verde con un rayo amarillo dentro
    g = G(16)
    g.disk(7.5, 8, 6.2, 6.8, 'g', 'G')
    g.outline()
    bolt = [(9, 3), (8, 4), (8, 5), (7, 6), (7, 7), (6, 8), (7, 8), (8, 8), (9, 8), (8, 9), (8, 10), (7, 11), (6, 12)]
    for x, y in bolt:
        g.px(x, y, 'f')
    for x, y in ((9, 4), (8, 6), (10, 8), (9, 9)):
        g.px(x, y, 'v')
    g.px(4, 4, 't'); g.px(4, 5, 't'); g.px(5, 3, 't')
    return g.rows()


def plasmaorb():
    # central, raro: una esfera de plasma violeta con rayos que salen del centro
    g = G(16)
    g.disk(7.5, 7.5, 6.8, 6.8, 'l', 'L')
    g.outline()
    g.disk(7.5, 7.5, 1.6, 1.6, 'w')
    for ang in (20, 95, 160, 230, 300):
        a = math.radians(ang)
        for k in range(2, 6):
            wig = 0.6 * math.sin(k * 2.1 + ang)
            x = 7.5 + k * math.cos(a) - wig * math.sin(a)
            y = 7.5 + k * math.sin(a) + wig * math.cos(a)
            g.px(int(round(x)), int(round(y)), 'u' if k > 3 else 'w')
    g.px(4, 3, 'w'); g.px(3, 4, 'w')
    return g.rows()


def galefeather():
    # torre del cielo, comun: una pluma blanca de borde gris (no la azul del prado), con tres rafagas de viento
    g = G(16)
    blade(g, 4, 15, 13, 1, 3.4, 'w', 's', 'S')
    g.outline()
    g.px(3, 15, 'k')
    for x0, y, n in ((0, 4, 5), (1, 8, 4), (0, 12, 5)):
        for x in range(x0, x0 + n):
            g.px(x, y, 'b')
        g.px(x0 + n, y - 1, 'b')
    return g.rows()


def cloudpuff():
    # torre del cielo, comun: una nube esponjosa, tres bultos sobre una base plana
    g = G(16)
    g.disk(4.5, 9.5, 3.4, 3.2, 'w')
    g.disk(8.5, 6.5, 4.2, 4.2, 'w')
    g.disk(12, 9, 3.0, 3.0, 'w')
    g.rect(2, 10, 13, 12, 'w')
    g.outline()
    g.rect(2, 11, 13, 11, 's')
    g.px(11, 10, 's'); g.px(12, 10, 's'); g.px(4, 10, 's')
    g.px(6, 4, 'b'); g.px(7, 3, 'b')
    return g.rows()


def windchime():
    # torre del cielo, poco comun: un carillon de viento: barra de madera, tubos de plata y la paleta azul
    g = G(16)
    g.rect(1, 2, 14, 4, 'c'); g.rect(1, 4, 14, 4, 'C')
    g.rect(7, 0, 8, 1, 'S')
    g.outline()
    for x0, ln in ((2, 6), (5, 9), (9, 8), (12, 5)):
        g.rect(x0, 6, x0 + 1, 5 + ln, 's'); g.px(x0 + 1, 5 + ln, 'S')
        g.px(x0, 5, 'S')
        g.px(x0, 6, 'w')
    g.rect(7, 12, 8, 15, 'b'); g.rect(7, 5, 7, 11, 'S')
    return g.rows()


def skypearl():
    # torre del cielo, poco comun: una perla azul cielo con una nubecita reflejada
    g = G(16)
    g.disk(7.5, 7.5, 6.4, 6.4, 'b', 'B')
    g.outline()
    g.rect(6, 9, 10, 10, 'w'); g.px(7, 8, 'w'); g.px(8, 8, 'w'); g.rect(6, 10, 10, 10, 's')
    g.px(4, 4, 'w'); g.px(5, 3, 'w'); g.px(4, 5, 'w')
    g.px(11, 11, 'N'); g.px(10, 12, 'N')
    return g.rows()


def starfragment():
    # torre del cielo, raro: un trozo de estrella dorada con destellos
    g = G(16)
    for y in range(16):
        for x in range(16):
            ang = math.atan2(y - 8.6, x - 7.5)
            r = math.hypot(x - 7.5, y - 8.6)
            k = (ang + math.pi / 2) % (2 * math.pi / 5)
            f = abs(k - math.pi / 5) / (math.pi / 5)
            lim = 6.9 - 3.9 * f
            if r <= lim and not (x >= 11 and y >= 11):  # una punta rota
                g.px(x, y, 'f' if (x - 7.5) + (y - 8.6) < 1.5 else 'Y')
    g.outline()
    g.px(7, 6, 'w'); g.px(6, 7, 'w'); g.px(7, 7, 'v')
    spark(g, 13, 2, 'v')
    g.px(1, 2, 'v'); g.px(14, 12, 'f')
    return g.rows()


def gear():
    # distrito toxico, comun: una rueda dentada de hierro con el agujero en medio
    g = G(16)
    for y in range(16):
        for x in range(16):
            r = math.hypot(x - 7.5, y - 7.5)
            a = math.degrees(math.atan2(y - 7.5, x - 7.5)) % 45
            if r <= 5.2 or (r <= 7.4 and 12 <= a <= 33):
                g.px(x, y, 's' if x + y < 15 else 'S')
    g.outline()
    g.disk(7.5, 7.5, 1.9, 1.9, '.')
    for y in range(16):
        for x in range(16):
            r = math.hypot(x - 7.5, y - 7.5)
            if 1.9 < r <= 2.9:
                g.px(x, y, 'k')
    g.px(4, 5, 'w'); g.px(5, 4, 'w')
    return g.rows()


def sludgevial():
    # distrito toxico, comun: un frasco de cristal con lodo violeta burbujeante
    g = G(16)
    g.rect(6, 0, 9, 2, 'c'); g.rect(9, 0, 9, 2, 'C')
    g.rect(6, 3, 9, 6, 'l'); g.rect(9, 3, 9, 6, 'L')
    g.disk(7.5, 10.5, 5.4, 4.6, 'l', 'L')
    g.outline()
    for y in range(9, 15):
        for x in range(16):
            if g.g[y][x] in ('l', 'L'):
                g.px(x, y, 'u' if x < 9 else 'U')
    g.px(5, 10, 'w'); g.px(8, 11, 'w'); g.px(10, 12, 'q')
    g.px(4, 7, 'w'); g.px(4, 8, 'w'); g.px(7, 4, 'w')
    return g.rows()


def gasmask():
    # distrito toxico, poco comun: una mascara de gas con dos visores y el filtro verde
    g = G(16)
    g.disk(7.5, 7, 6.2, 6.2, 'S', 'k')
    g.rect(5, 11, 10, 15, 'G'); g.rect(9, 11, 10, 15, 'd')
    g.outline()
    g.disk(4.8, 6, 1.8, 1.8, 'b'); g.disk(10.2, 6, 1.8, 1.8, 'b')
    g.px(4, 5, 'w'); g.px(9, 5, 'w')
    g.rect(6, 13, 9, 13, 'k'); g.rect(6, 12, 9, 12, 'g')
    g.px(3, 2, 's'); g.px(4, 2, 's'); g.px(2, 3, 's')
    return g.rows()


def toxiccrystal():
    # distrito toxico, poco comun: un racimo de cristales magenta
    g = G(16)
    for x0, x1, ytop, tip in ((1, 5, 6, 2), (10, 14, 5, 2), (4, 11, 1, 3)):
        for y in range(ytop, 15):
            inset = max(0, tip - (y - ytop))
            for x in range(x0 + inset, x1 - inset + 1):
                rel = x - x0 - inset
                wid = x1 - x0 - 2 * inset
                g.px(x, y, 'p' if rel <= wid // 3 else ('Q' if rel >= wid - wid // 3 and wid > 1 else 'q'))
    g.outline()
    g.px(6, 3, 'w'); g.px(6, 4, 'w'); g.px(2, 8, 'w')
    g.rect(1, 15, 14, 15, 'k')
    return g.rows()


def rustedcrown():
    # distrito toxico, raro: una corona vieja y oxidada, con una gema verde que aun brilla
    g = G(16)
    g.rect(2, 8, 13, 13, 'O')
    for cx in (2, 7, 13):
        for y in range(3, 8):
            w = (y - 3) // 2
            g.rect(cx - w, y, cx + w + (1 if cx == 7 else 0), y, 'O')
    g.rect(2, 12, 13, 13, 'C')
    g.outline()
    g.rect(3, 9, 12, 9, 'f')
    for x, y in ((4, 10), (5, 11), (10, 10), (11, 11), (8, 6), (12, 7), (3, 6), (9, 11)):
        g.px(x, y, 'c')
    g.px(7, 10, 'g'); g.px(8, 10, 'g'); g.px(7, 11, 'G'); g.px(8, 11, 'G'); g.px(7, 10, 'w')
    g.px(2, 3, 'f'); g.px(13, 3, 'f'); g.px(7, 3, 'f'); g.px(8, 3, 'f')
    return g.rows()


def suitcase():
    # la maleta del viaje (tarjeta de Jugar, marca de la caja): marron con asa, dos cierres y una pegatina
    g = G(16)
    g.rect(1, 5, 14, 14, 'c'); g.rect(13, 5, 14, 14, 'C'); g.rect(1, 12, 14, 14, 'C')
    g.px(1, 5, '.'); g.px(14, 5, '.')
    g.outline()
    g.rect(4, 0, 11, 4, '.')
    g.rect(5, 1, 10, 1, 'y'); g.rect(5, 2, 5, 4, 'y'); g.rect(10, 2, 10, 4, 'y')
    g.rect(5, 1, 5, 1, 'Y'); g.rect(10, 2, 10, 4, 'Y'); g.rect(10, 1, 10, 1, 'Y')
    g.rect(2, 8, 13, 8, 'C')
    g.rect(4, 7, 5, 9, 'f'); g.rect(10, 7, 11, 9, 'f'); g.px(4, 7, 'w')
    g.rect(7, 10, 9, 12, 'b'); g.px(8, 11, 'w')
    g.px(3, 6, 'y'); g.px(2, 6, 'y')
    return g.rows()


ICONS = [
    ("CLOVER", clover), ("DAISY", daisy), ("FEATHER", feather), ("HONEY", honey), ("ACORN", acorn),
    ("PEBBLE", pebble), ("QUARTZ", quartz), ("FOSSIL", fossil), ("ORE", ore), ("MOONSTONE", moonstone),
    ("PINECONE", pinecone), ("MUSHROOM", mushroom), ("OWLFEATHER", owlfeather), ("AMBER", amber), ("LEAF", ancientleaf),
    ("SEASHELL", seashell), ("STARFISH", starfish), ("SEAGLASS", seaglass), ("BOTTLE", bottle), ("PEARL", pearl),
    ("COIN", oldcoin), ("MASK", mask), ("CANDLE", candle), ("TABLET", tablet), ("LANTERN", lantern),
    # v3.15: volcan, central, torre del cielo, distrito toxico
    ("EMBER", ember), ("OBSIDIAN", obsidian), ("MAGMA", magmastone), ("FIREFEATHER", firefeather), ("PHOENIX", phoenixember),
    ("BATTERY", battery), ("MAGNET", magnet), ("COIL", coppercoil), ("THUNDER", thunderstone), ("PLASMA", plasmaorb),
    ("GALEFEATHER", galefeather), ("CLOUD", cloudpuff), ("CHIME", windchime), ("SKYPEARL", skypearl), ("STAR", starfragment),
    ("GEAR", gear), ("VIAL", sludgevial), ("GASMASK", gasmask), ("TOXCRYSTAL", toxiccrystal), ("CROWN", rustedcrown),
]
MAPS = 9            # = TRV_DESTS (travel.h)
PER_MAP = 5         # = SOUV_PER_DEST
COUNT = MAPS * PER_MAP  # = SOUV_COUNT


def build():
    return [(n, f()) for n, f in ICONS]


def validate(icons):
    ok = len(icons) == COUNT
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
    nrows = (len(icons) + cols - 1) // cols
    img = Image.new('RGB', (cols * cell + 12, nrows * cell + 12), '#14161E')
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
           f"// Los {COUNT} recuerdos de los viajes (v2.0, Y5; v3.15: {MAPS} mapas), 16x16, en el orden de sus ids; los\n",
           "// caracteres son los de spriteColor (species.h). Mismo formato que los iconos de objeto.\n\n"]
    for n, rows in icons:
        out.append(f"static const char* const SPR_SOUV_{n}[16] = {{\n")
        for r in rows:
            out.append(f'  "{r}",\n')
        out.append("};\n\n")
    out.append("static const char* const SPR_ICON_TRAVEL[16] = {\n")
    for r in suitcase():
        out.append(f'  "{r}",\n')
    out.append("};\n\n")
    out.append(f"static const char *const *const SOUV_ICON[{COUNT}] = {{\n")
    for i in range(0, COUNT, PER_MAP):
        out.append("  " + ", ".join(f"SPR_SOUV_{n}" for n, _ in icons[i:i + PER_MAP]) + ",\n")
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
        render([("SUITCASE", suitcase())], sys.argv[sys.argv.index('png') + 1].replace('.png', '-case.png'), 8)
