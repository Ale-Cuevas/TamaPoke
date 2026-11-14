#!/usr/bin/env python3
"""Taller de sprites de TamaPoke.

Los sprites 32x32 se CONSTRUYEN con primitivas (elipses con sombreado
automatico en el borde inferior-derecho, contorno automatico de silueta)
y detalles a pixel (ojos, boca, manchas). El script valida, renderiza un
contact-sheet PNG para revision visual, y emite los arrays C y JS:

  python3 tools/sprites.py        # valida + renderiza tools/sheet.png
  python3 tools/sprites.py emit   # regenera species.h y emitted_sprites.js
"""
from PIL import Image, ImageDraw

PALETTE = {
    'k': '#1b1b25',  # contorno / ojos
    'w': '#ffffff',  # brillo de ojo
    'y': '#f8d990',  # crema (barriga, placa caparazon)
    'Y': '#e0b860',  # crema sombra
    'o': '#f5863d',  # naranja
    'O': '#d65f28',  # naranja sombra
    'r': '#e8503a',  # rojo (llama, charmeleon)
    'R': '#b53224',  # rojo sombra
    'f': '#ffd95e',  # amarillo llama
    't': '#8fd6b4',  # menta (bulbasaur)
    'T': '#5fae8c',  # menta sombra
    'g': '#58b868',  # verde bulbo
    'G': '#3c8a4c',  # verde bulbo sombra
    'd': '#3f7e62',  # manchas bulbasaur
    'p': '#f08aa4',  # rosa flor
    'P': '#c75f80',  # rosa sombra
    'b': '#7cc4ea',  # azul (squirtle)
    'B': '#4f93c4',  # azul sombra
    'N': '#3a6fa0',  # azul blastoise
    'M': '#2a5278',  # azul blastoise sombra
    'c': '#b07a45',  # marron caparazon
    'C': '#7e5530',  # marron sombra
    'l': '#9aa9e0',  # lavanda (wartortle)
    'L': '#6f7cb8',  # lavanda sombra
    's': '#aab0bc',  # gris canon
    'S': '#787e8c',  # gris sombra
}

W = H = 32


class G:
    def __init__(self, size=W):
        self.size = size
        self.g = [['.'] * size for _ in range(size)]

    def disk(self, cx, cy, rx, ry, ch, shade=None):
        for y in range(self.size):
            for x in range(self.size):
                dx, dy = (x - cx) / rx, (y - cy) / ry
                n = dx * dx + dy * dy
                if n <= 1.0:
                    c = ch
                    if shade and n >= 0.45 and (dx + dy) > 0.42:
                        c = shade
                    self.g[y][x] = c

    def rect(self, x0, y0, x1, y1, ch):
        for y in range(max(0, y0), min(self.size, y1 + 1)):
            for x in range(max(0, x0), min(self.size, x1 + 1)):
                self.g[y][x] = ch

    def px(self, x, y, ch):
        if 0 <= x < self.size and 0 <= y < self.size:
            self.g[y][x] = ch

    def erase(self, cx, cy, rx, ry):
        for y in range(self.size):
            for x in range(self.size):
                dx, dy = (x - cx) / rx, (y - cy) / ry
                if dx * dx + dy * dy <= 1.0:
                    self.g[y][x] = '.'

    def outline(self):
        out = [row[:] for row in self.g]
        for y in range(self.size):
            for x in range(self.size):
                if self.g[y][x] == '.':
                    continue
                for nx, ny in ((x-1, y), (x+1, y), (x, y-1), (x, y+1)):
                    if not (0 <= nx < self.size and 0 <= ny < self.size) or self.g[ny][nx] == '.':
                        out[y][x] = 'k'
                        break
        self.g = out

    def eye(self, x, y):
        # ojo 3x4 con brillo
        self.rect(x, y, x + 2, y + 3, 'k')
        self.px(x, y + 1, 'w')
        self.px(x + 1, y, 'w')

    def smile(self, cx, row):
        self.px(cx - 3, row, 'k')
        self.rect(cx - 2, row + 1, cx + 2, row + 1, 'k')
        self.px(cx + 3, row, 'k')

    def toes(self, x, y):
        self.px(x, y, 'k')

    def rows(self):
        return [''.join(r) for r in self.g]


# ---------------------------------------------------------------- linea fuego

def charmander():
    g = G()
    # cola gruesa subiendo por la derecha, con llama
    g.rect(21, 23, 25, 26, 'o')
    g.rect(24, 19, 27, 24, 'O')
    g.rect(26, 15, 28, 20, 'O')
    g.disk(27.5, 12, 3.4, 4.2, 'r', 'R')
    g.px(27, 7, 'r'); g.px(28, 8, 'r')
    g.disk(27.2, 13.2, 1.6, 2.2, 'f')
    # cabeza grande + cuerpo
    g.disk(14, 9.5, 8.2, 7.4, 'o', 'O')
    g.disk(14, 22, 7.0, 6.2, 'o', 'O')
    # brazos
    g.rect(5, 18, 7, 22, 'o')
    g.rect(21, 18, 23, 20, 'o')
    # patas
    g.rect(7, 27, 11, 30, 'o')
    g.rect(17, 27, 21, 30, 'o')
    g.outline()
    # barriga crema
    g.disk(14, 22.5, 4.6, 4.2, 'y', 'Y')
    # deditos
    g.toes(9, 30); g.toes(19, 30)
    g.eye(9, 6); g.eye(17, 6)
    g.smile(13, 12)
    return g.rows()


def charmeleon():
    g = G()
    # cola larga con llama grande
    g.rect(21, 22, 25, 25, 'r')
    g.rect(24, 17, 27, 23, 'R')
    g.disk(27, 12.5, 3.8, 4.6, 'r', 'R')
    g.px(26, 6, 'r'); g.px(27, 7, 'r'); g.px(28, 6, 'r')
    g.disk(26.8, 13.8, 1.8, 2.4, 'f')
    # cuerno hacia atras
    g.px(19, 2, 'r'); g.px(20, 1, 'r'); g.px(21, 0, 'r')
    g.rect(18, 3, 20, 4, 'r'); g.px(20, 2, 'r'); g.px(21, 1, 'r')
    # cabeza + cuerpo esbelto
    g.disk(13, 9.5, 7.8, 7.0, 'r', 'R')
    g.disk(13, 22, 6.4, 6.2, 'r', 'R')
    # brazos con garra
    g.rect(4, 17, 6, 22, 'r')
    g.rect(20, 17, 22, 20, 'r')
    # patas
    g.rect(6, 27, 10, 30, 'r')
    g.rect(16, 27, 20, 30, 'r')
    g.outline()
    g.disk(13, 22.5, 4.2, 4.2, 'y', 'Y')
    g.toes(8, 30); g.toes(18, 30)
    g.eye(8, 6); g.eye(16, 6)
    g.smile(12, 12)
    return g.rows()


def charizard():
    g = G()
    # alas: membrana verdosa con pico alto (detras)
    for i, (x0, x1) in enumerate([(1, 3), (1, 4), (1, 5), (2, 6), (2, 7), (2, 7),
                                  (3, 8), (3, 8), (3, 9), (4, 9), (4, 9), (5, 9)]):
        g.rect(x0, 4 + i, x1, 4 + i, 'T')
    for i, (x0, x1) in enumerate([(28, 30), (27, 30), (26, 30), (25, 29), (24, 29), (24, 29),
                                  (23, 28), (23, 28), (22, 28), (22, 27), (22, 27), (22, 26)]):
        g.rect(x0, 4 + i, x1, 4 + i, 'T')
    g.px(1, 3, 'T'); g.px(30, 3, 'T')
    # cola con llama
    g.rect(22, 25, 26, 27, 'o')
    g.rect(25, 21, 28, 26, 'O')
    g.disk(28, 18, 3.0, 3.8, 'r', 'R')
    g.px(28, 13, 'r'); g.px(27, 14, 'r')
    g.disk(27.8, 19.2, 1.4, 2.0, 'f')
    # cuerpo macizo + cabeza
    g.disk(15, 23, 8.4, 7.0, 'o', 'O')
    g.disk(15, 9, 7.0, 6.2, 'o', 'O')
    # cuernos
    g.rect(10, 1, 11, 3, 'o'); g.rect(19, 1, 20, 3, 'o')
    # patas anchas
    g.rect(7, 28, 12, 31, 'o')
    g.rect(18, 28, 23, 31, 'o')
    g.outline()
    g.disk(15, 23.5, 5.2, 4.8, 'y', 'Y')
    g.toes(9, 31); g.toes(20, 31)
    g.eye(10, 6); g.eye(18, 6)
    g.smile(14, 12)
    return g.rows()


# --------------------------------------------------------------- linea planta

def bulbasaur():
    g = G()
    # bulbo con brote
    g.disk(21.5, 8.5, 7.2, 6.4, 'g', 'G')
    g.rect(20, 1, 22, 2, 'G')
    # cuerpo ancho (cuadrupedo)
    g.disk(14, 22, 9.6, 6.4, 't', 'T')
    # cabeza
    g.disk(10, 13, 7.8, 6.2, 't', 'T')
    # orejas
    g.px(3, 4, 't'); g.rect(3, 5, 5, 8, 't'); g.px(4, 4, 't')
    g.px(13, 3, 't'); g.rect(12, 4, 14, 7, 't'); g.px(14, 3, 't')
    # patas
    g.rect(4, 26, 7, 30, 't')
    g.rect(10, 26, 13, 30, 't')
    g.rect(16, 26, 19, 30, 't')
    g.rect(21, 25, 24, 29, 't')
    g.outline()
    # hojas abrazando la base del bulbo
    g.px(15, 13, 'G'); g.px(16, 14, 'G'); g.px(17, 14, 'G')
    g.px(26, 13, 'G'); g.px(25, 14, 'G')
    # manchas
    g.rect(7, 7, 8, 7, 'd'); g.rect(13, 17, 14, 18, 'd')
    g.rect(5, 21, 6, 22, 'd'); g.rect(17, 22, 18, 23, 'd')
    g.toes(5, 30); g.toes(11, 30); g.toes(17, 30)
    g.eye(6, 10); g.eye(13, 10)
    g.smile(9, 16)
    return g.rows()


def ivysaur():
    g = G()
    # bulbo con capullo rosa y hojas
    g.disk(21.5, 9.5, 7.0, 5.8, 'g', 'G')
    g.disk(21.5, 3.2, 3.6, 3.0, 'p', 'P')
    g.px(21, 0, 'p'); g.px(22, 0, 'p')
    # hojas laterales
    g.rect(15, 6, 17, 7, 'g'); g.px(14, 5, 'g'); g.px(15, 5, 'g')
    g.rect(28, 6, 30, 7, 'g'); g.px(30, 5, 'g')
    g.disk(14, 22, 9.6, 6.4, 't', 'T')
    g.disk(10, 13, 7.8, 6.2, 't', 'T')
    g.px(3, 4, 't'); g.rect(3, 5, 5, 8, 't'); g.px(4, 4, 't')
    g.px(13, 3, 't'); g.rect(12, 4, 14, 7, 't'); g.px(14, 3, 't')
    g.rect(4, 26, 7, 30, 't')
    g.rect(10, 26, 13, 30, 't')
    g.rect(16, 26, 19, 30, 't')
    g.rect(21, 25, 24, 29, 't')
    g.outline()
    # division de petalos del capullo
    g.px(21, 1, 'P'); g.px(21, 2, 'P')
    g.px(15, 13, 'G'); g.px(16, 14, 'G')
    g.rect(7, 7, 8, 7, 'd'); g.rect(13, 17, 14, 18, 'd')
    g.rect(5, 21, 6, 22, 'd')
    g.toes(5, 30); g.toes(11, 30); g.toes(17, 30)
    g.eye(6, 10); g.eye(13, 10)
    g.smile(9, 16)
    return g.rows()


def venusaur():
    g = G()
    # flor abierta: petalos rosas + centro amarillo sobre hojas
    g.disk(21, 5.5, 7.0, 4.2, 'p', 'P')
    g.disk(21, 6.5, 2.8, 2.0, 'f')
    # separaciones de petalos
    g.px(17, 2, 'P'); g.px(17, 3, 'P'); g.px(25, 2, 'P'); g.px(25, 3, 'P')
    # corona de hojas
    g.disk(21, 11, 9.0, 3.2, 'g', 'G')
    # cuerpo macizo
    g.disk(15, 23, 11.0, 7.0, 't', 'T')
    g.disk(9, 13.5, 7.6, 6.0, 't', 'T')
    g.px(2, 5, 't'); g.rect(2, 6, 4, 9, 't'); g.px(3, 5, 't')
    g.px(12, 4, 't'); g.rect(11, 5, 13, 8, 't'); g.px(13, 4, 't')
    # patas tronco
    g.rect(3, 26, 7, 30, 't')
    g.rect(10, 26, 14, 30, 't')
    g.rect(17, 26, 21, 30, 't')
    g.rect(23, 25, 27, 29, 't')
    g.outline()
    g.rect(6, 7, 7, 7, 'd'); g.rect(12, 17, 13, 18, 'd')
    g.rect(5, 22, 6, 23, 'd'); g.rect(18, 22, 19, 23, 'd')
    g.toes(5, 30); g.toes(12, 30); g.toes(19, 30)
    g.eye(5, 10); g.eye(12, 10)
    g.smile(8, 16)
    return g.rows()


# ---------------------------------------------------------------- linea agua

def squirtle():
    g = G()
    # cola en espiral (anillo con hueco)
    g.disk(23.5, 20, 4.6, 5.2, 'b', 'B')
    g.erase(24.7, 19.2, 1.6, 1.9)
    # cabeza grande + cuerpo
    g.disk(13, 9, 8.4, 7.6, 'b', 'B')
    g.disk(13, 22.5, 7.0, 5.8, 'b', 'B')
    # brazos
    g.rect(4, 17, 6, 22, 'b')
    g.rect(20, 17, 22, 19, 'b')
    # patas
    g.rect(6, 27, 10, 30, 'b')
    g.rect(16, 27, 20, 30, 'b')
    g.outline()
    # caparazon: aro marron + placa crema
    g.disk(13, 22.5, 5.2, 4.4, 'c', 'C')
    g.disk(13, 22.5, 3.6, 3.0, 'y', 'Y')
    g.toes(8, 30); g.toes(18, 30)
    g.eye(8, 5); g.eye(16, 5)
    g.smile(12, 11)
    return g.rows()


def wartortle():
    g = G()
    # cola esponjosa en nube
    g.disk(23.5, 18, 5.4, 7.0, 'l', 'L')
    # ondas en la cola
    g.px(22, 14, 'L'); g.px(23, 15, 'L'); g.px(24, 16, 'L')
    g.px(21, 19, 'L'); g.px(22, 20, 'L')
    # cabeza + cuerpo
    g.disk(12, 9.5, 7.8, 7.0, 'l', 'L')
    g.disk(12, 22.5, 6.6, 5.6, 'l', 'L')
    # orejas onduladas
    g.px(4, 0, 'l'); g.rect(3, 1, 5, 5, 'l'); g.px(2, 2, 'l')
    g.px(18, 0, 'l'); g.rect(17, 1, 19, 5, 'l'); g.px(20, 2, 'l')
    # brazos
    g.rect(3, 17, 5, 21, 'l')
    g.rect(19, 17, 21, 19, 'l')
    # patas
    g.rect(5, 27, 9, 30, 'l')
    g.rect(15, 27, 19, 30, 'l')
    g.outline()
    # interior de las orejas
    g.px(4, 2, 'L'); g.px(4, 3, 'L'); g.px(18, 2, 'L'); g.px(18, 3, 'L')
    g.disk(12, 22.5, 4.8, 4.2, 'c', 'C')
    g.disk(12, 22.5, 3.2, 2.8, 'y', 'Y')
    g.toes(7, 30); g.toes(17, 30)
    g.eye(7, 6); g.eye(15, 6)
    g.smile(11, 12)
    return g.rows()


def blastoise():
    g = G()
    # canones inclinados sobre los hombros
    g.rect(3, 4, 6, 7, 'S')
    g.rect(4, 7, 7, 18, 's')
    g.px(4, 5, 'k'); g.px(5, 5, 'k')
    g.rect(25, 4, 28, 7, 'S')
    g.rect(24, 7, 27, 18, 's')
    g.px(26, 5, 'k'); g.px(27, 5, 'k')
    # cabeza + cuerpo macizo
    g.disk(15.5, 8.5, 7.0, 6.2, 'N', 'M')
    g.disk(15.5, 22, 9.4, 7.2, 'N', 'M')
    # brazos fornidos
    g.rect(5, 19, 8, 25, 'N')
    g.rect(23, 19, 26, 25, 'N')
    # patas anchas
    g.rect(7, 28, 12, 31, 'N')
    g.rect(19, 28, 24, 31, 'N')
    g.outline()
    g.disk(15.5, 22.5, 6.0, 5.0, 'c', 'C')
    g.disk(15.5, 22.5, 4.2, 3.6, 'y', 'Y')
    g.toes(9, 31); g.toes(21, 31)
    g.eye(11, 6); g.eye(19, 6)
    g.smile(15, 12)
    return g.rows()


# ------------------------------------------------------------------ genericos

def egg():
    g = G()
    g.disk(16, 17, 8.6, 11.0, 'y', 'Y')
    g.outline()
    g.rect(12, 11, 13, 12, 'g'); g.px(12, 13, 'g')
    g.rect(19, 16, 20, 17, 'g'); g.px(20, 18, 'g')
    g.rect(13, 22, 14, 23, 'g')
    return g.rows()


def poop():
    g = G()
    g.disk(16, 24, 7.0, 4.4, 'c', 'C')
    g.disk(16, 18, 5.0, 3.4, 'c', 'C')
    g.disk(16, 13, 3.0, 2.4, 'c', 'C')
    g.px(16, 9, 'c'); g.px(17, 10, 'c')
    g.outline()
    return g.rows()


def heart():
    # Dibujado a mano y editado directamente en species.h (commit 7c2b0f9,
    # "animaciones epicas, corazon y biomas"), sin volver aqui: el generador
    # se quedo con una version procedural distinta y species.h dejo de poder
    # regenerarse igual. Se fija aqui el corazon que de verdad se envia.
    return [
        "................................",
        "................................",
        "................................",
        "................................",
        "................................",
        "................................",
        "........krrrrk....krrrrk........",
        ".......krrrrrrk..krrrrrrk.......",
        "......krrrrrrrrrrrrrrrrrrk......",
        ".....krrrrrrrrrrrrrrrrrrrrk.....",
        ".....krrwwrrrrrrrrrrrrrrRrk.....",
        ".....krwwrrrrrrrrrrrrrrRRrk.....",
        "......krrrrrrrrrrrrrrrrrrk......",
        ".......krrrrrrrrrrrrrrrrk.......",
        "........krrrrrrrrrrrrrrk........",
        ".........krrrrrrrrrrrrk.........",
        "..........krrrrrrrrrrk..........",
        "...........krrrrrrrrk...........",
        "............krrrrrrk............",
        ".............krrrrk.............",
        "..............krrk..............",
        "...............kk...............",
        "................................",
        "................................",
        "................................",
        "................................",
        "................................",
        "................................",
        "................................",
        "................................",
        "................................",
        "................................",
    ]


# ------------------------------------------------------------ iconos botones

def icon_food():
    g = G(16)
    g.disk(8, 9.5, 5.2, 4.8, 'r', 'R')
    g.px(8, 3, 'C'); g.px(8, 4, 'C')
    g.px(10, 3, 'g'); g.px(11, 3, 'g'); g.px(11, 2, 'g')
    g.outline()
    g.px(6, 7, 'w'); g.px(5, 8, 'w')
    return g.rows()


def icon_play():
    g = G(16)
    g.disk(8, 8, 6.2, 6.2, 'r', 'R')
    g.disk(8, 11.4, 5.4, 3.2, 'w', 'Y')
    g.outline()
    g.rect(3, 8, 13, 8, 'k')
    g.px(2, 8, 'k'); g.px(14, 8, 'k')
    g.rect(7, 7, 9, 9, 'k')
    g.px(8, 8, 'w')
    return g.rows()


def icon_light():
    g = G(16)
    g.disk(8, 8, 5.8, 5.8, 'f')
    g.erase(11, 5.5, 4.6, 4.6)
    g.outline()
    g.px(12, 3, 'f'); g.px(13, 5, 'f')
    return g.rows()


def icon_berry_blue():
    g = G(16)
    g.disk(8, 9.5, 5.2, 4.8, 'b', 'B')
    g.px(8, 3, 'C'); g.px(8, 4, 'C')
    g.px(10, 3, 'g'); g.px(11, 2, 'g')
    g.outline()
    g.px(6, 7, 'w'); g.px(5, 8, 'w')
    return g.rows()


def icon_berry_green():
    g = G(16)
    g.disk(8, 9.5, 5.2, 4.8, 'g', 'G')
    g.px(8, 3, 'C'); g.px(8, 4, 'C')
    g.px(10, 3, 't'); g.px(11, 2, 't')
    g.outline()
    g.px(6, 7, 'w'); g.px(5, 8, 'w')
    return g.rows()


def icon_candy():
    g = G(16)
    g.disk(8, 8, 4.4, 3.6, 'p', 'P')
    # envoltorio: picos a los lados
    g.px(2, 6, 'p'); g.px(2, 8, 'p'); g.px(2, 10, 'p'); g.rect(3, 7, 3, 9, 'p')
    g.px(13, 6, 'p'); g.px(13, 8, 'p'); g.px(13, 10, 'p'); g.rect(12, 7, 12, 9, 'p')
    g.outline()
    g.px(7, 6, 'w'); g.px(6, 7, 'w')
    g.px(8, 8, 'P'); g.px(9, 9, 'P')
    return g.rows()


def icon_clean():
    g = G(16)
    g.disk(7, 10, 4.6, 4.4, 'b', 'B')
    g.disk(12, 4.5, 2.2, 2.2, 'b', 'B')
    g.disk(3.5, 4, 1.4, 1.4, 'b')
    g.outline()
    g.px(5, 8, 'w'); g.px(6, 9, 'w'); g.px(11, 4, 'w')
    return g.rows()


# ------------------------------------------------- objetos de la mochila (pasos)

def icon_golden():
    # baya dorada: la baya de siempre, en oro y con destellos
    g = G(16)
    g.disk(8, 9.5, 5.2, 4.8, 'f', 'Y')
    g.px(8, 3, 'C'); g.px(8, 4, 'C')
    g.px(10, 3, 'g'); g.px(11, 3, 'g'); g.px(11, 2, 'g')
    g.outline()
    g.px(6, 7, 'w'); g.px(5, 8, 'w')
    g.px(1, 3, 'f'); g.px(2, 2, 'w'); g.px(3, 3, 'f'); g.px(2, 4, 'f')
    g.px(14, 12, 'w'); g.px(13, 13, 'f'); g.px(15, 13, 'f')
    return g.rows()


def _vitamin(body, shade):
    # frasco de vitamina: tapon, cuello, cuerpo del color y etiqueta blanca
    g = G(16)
    g.rect(6, 1, 9, 2, 'c')
    g.rect(7, 3, 8, 3, 'y')
    g.disk(7.5, 9.5, 5.0, 5.6, body, shade)
    g.rect(4, 4, 11, 4, body)
    g.outline()
    g.rect(4, 8, 11, 10, 'w')
    g.rect(6, 9, 9, 9, shade)
    g.px(5, 6, 'w')
    return g.rows()


def icon_protein():
    return _vitamin('r', 'R')  # ataque: rojo


def icon_iron():
    return _vitamin('s', 'S')  # defensa: gris hierro


def icon_carbos():
    return _vitamin('g', 'G')  # velocidad: verde


def icon_charm():
    # amuleto shiny: medallon lavanda colgado de un cordon, con una estrella
    g = G(16)
    g.disk(8, 9.5, 5.4, 5.4, 'l', 'L')
    g.outline()
    g.px(7, 2, 'k'); g.px(8, 2, 'k'); g.px(6, 3, 'k'); g.px(9, 3, 'k')
    g.px(6, 4, 'k'); g.px(9, 4, 'k')
    g.rect(8, 6, 8, 13, 'f')
    g.rect(5, 9, 11, 10, 'f')
    g.rect(7, 8, 9, 11, 'f')
    g.px(8, 9, 'w'); g.px(8, 10, 'w')
    g.px(13, 3, 'w'); g.px(14, 4, 'f'); g.px(12, 4, 'f'); g.px(13, 5, 'f')
    return g.rows()


def icon_rarecandy():
    # caramelo raro (v1.25): caramelo azul envuelto, puntas lavanda y un destello
    g = G(16)
    g.disk(8, 8.5, 4.6, 4.3, 'b', 'B')
    for i in range(3):  # puntas del envoltorio: abanicos que se abren hacia fuera
        g.rect(3 - i, 7 - i, 3 - i, 10 + i, 'l')
        g.rect(12 + i, 7 - i, 12 + i, 10 + i, 'l')
    g.outline()
    g.rect(2, 7, 2, 10, 'L'); g.rect(13, 7, 13, 10, 'L')  # sombra del pliegue
    g.px(6, 6, 'w'); g.px(7, 6, 'w'); g.px(6, 7, 'w')       # brillo
    g.px(11, 0, 'w'); g.px(10, 1, 'f'); g.px(12, 1, 'f'); g.px(11, 2, 'w')  # destello
    return g.rows()


def icon_flag():
    # banderin de la meta de pasos
    g = G(16)
    g.rect(3, 1, 4, 14, 'C')
    for y in range(2, 9):
        w = 9 - abs(y - 5) * 2
        g.rect(5, y, 5 + w, y, 'r')
    g.outline()
    g.rect(6, 4, 7, 4, 'w')
    g.rect(1, 14, 7, 14, 'k')
    return g.rows()


def icon_battle():
    # dos espadas cruzadas (menu de JUGAR: combate). Las hojas son finas, asi que
    # el contorno crece por fuera en vez de comerse la figura como outline()
    g = G(16)
    for i in range(1, 10):
        g.px(i, i, 'w'); g.px(i + 1, i, 's')            # hoja \
        g.px(15 - i, i, 'w'); g.px(14 - i, i, 's')      # hoja /
    for x, y in ((12, 9), (11, 10), (10, 11), (9, 12)):  # guardas doradas
        g.px(x, y, 'f'); g.px(15 - x, y, 'f')
    for x, y, c in ((11, 11, 'c'), (12, 12, 'c'), (13, 13, 'C')):  # empunaduras
        g.px(x, y, c); g.px(15 - x, y, c)
    grown = [row[:] for row in g.g]
    for y in range(16):
        for x in range(16):
            if g.g[y][x] != '.':
                continue
            if any(0 <= x + dx < 16 and 0 <= y + dy < 16 and g.g[y + dy][x + dx] != '.'
                   for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                grown[y][x] = 'k'
    g.g = grown
    return g.rows()


def icon_sack():
    # saco de entrenamiento rojo colgado de su cadena (menu de JUGAR, v1.34), como el de
    # la escena del saco: cuerpo rojo, tapa y costura mas oscuras
    g = G(16)
    g.rect(4, 3, 11, 15, 'r')
    for x, y in ((4, 3), (11, 3), (4, 15), (11, 15)):  # esquinas redondeadas
        g.px(x, y, '.')
    g.rect(10, 5, 10, 14, 'R')         # sombra a la derecha
    g.rect(5, 3, 10, 4, 'R')           # tapa
    g.rect(4, 9, 11, 9, 'R')           # costura
    g.outline()
    g.rect(6, 6, 6, 7, 'p')            # brillo
    g.rect(7, 0, 8, 2, 'S')            # cadena
    g.px(7, 1, 's')
    return g.rows()


# ------------------------------------- iconos dibujados a mano en species.h
# Se anadieron a species.h a mano (v1.39, v1.43, v1.44) sin pasar por aqui; se
# fijan tal cual para que "emit" no los borre.

def icon_egg():
    # huevo de la mochila (v1.39): el de la pantalla principal (SPR_EGG) en pequeno
    return [
        "................",
        "......kkkk......",
        ".....kwyyyk.....",
        "....kwyyyyyk....",
        "....kyyyggyk....",
        "...kyyyyggyYk...",
        "...kyyyyyyyYk...",
        "...kyggyyyyYk...",
        "...kyggyyyyYk...",
        "...kyyyyyggYk...",
        "...kyyyyyggYk...",
        "....kyyyyyYk....",
        "....kYyyyYYk....",
        ".....kYYYYk.....",
        "......kkkk......",
        "................",
    ]


def icon_dash():
    # trineo con lineas de velocidad (v1.44)
    return [
        "................",
        "................",
        "................",
        "bbb.............",
        "........kkkkkk..",
        ".bbbb..krrrrrrk.",
        ".......krrrrrrk.",
        "bbb...kRRRRRRRRk",
        "......kRrrrrrrRk",
        ".bbbb.kRRRRRRRRk",
        ".......kkkkkkkk.",
        "........S....S..",
        "bbb...SSSSSSSSSS",
        ".....S..........",
        "....S...........",
        "................",
    ]


def icon_zen():
    # cascada sobre la poza (v1.43)
    return [
        "SSSSSbbbbbbSSSSS",
        "SsSSSbwbbBbSSsSS",
        "SSSsSbwbbBbSsSSS",
        "SSSSSbbbwBbSSSSS",
        ".SSsSbbbwBbSsSS.",
        ".SSSSbwbbBbSSSS.",
        "..SSSbwbbBbSSS..",
        "..SsSbbbwBbSsS..",
        "...SSbbbwBbSS...",
        "...SSbwbbBbSS...",
        "....SbwbbBbS....",
        "....wbbbwBbw....",
        "..wwwwbbbbwwww..",
        ".wbbwbbwbbwbbww.",
        "NNNNNNNNNNNNNNNN",
        ".NNNNNNNNNNNNNN.",
    ]


# ------------------------------------------- iconos del boton mochila / cuidar

def _ring(g):
    # contorno que crece POR FUERA de la figura (para trazos finos como rayos)
    grown = [row[:] for row in g.g]
    for y in range(g.size):
        for x in range(g.size):
            if g.g[y][x] != '.':
                continue
            if any(0 <= x + dx < g.size and 0 <= y + dy < g.size and g.g[y + dy][x + dx] != '.'
                   for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                grown[y][x] = 'k'
    g.g = grown


def icon_bag():
    # mochila: asa arriba, solapa marron con hebilla dorada, cuerpo naranja y bolsillo
    return [
        "................",
        "......kkkk......",
        ".....k....k.....",
        "...kkkkkkkkkk...",
        "..kcwcccccccCk..",
        "..kcccccccccCk..",
        "..kccccccccCCk..",
        "..kkccccccCCkk..",
        "..kokkkffkkkOk..",
        "..koookfYkooOk..",
        "..kooookkoooOk..",
        "..koOOOOOOOOOk..",
        "..koOooooooOOk..",
        "..koOooooooOOk..",
        "..kOOOOOOOOOOk..",
        "...kkkkkkkkkk...",
    ]


def icon_care():
    # cuidar (dormir + limpiar): corazon rosa con una cruz blanca
    return [
        "................",
        "................",
        "...kkk....kkk...",
        "..kpppk..kpppk..",
        ".kpwpppkkppppPk.",
        ".kwppppwwppppPk.",
        ".kpppppwwpppPPk.",
        ".kpppwwwwwwppPk.",
        "..kppwwwwwwpPk..",
        "...kpppwwppPk...",
        "....kppwwpPk....",
        ".....kppPPk.....",
        "......kPPk......",
        ".......kk.......",
        "................",
        "................",
    ]


def icon_sun():
    # sol (despertar), junto a la luna de ICON_LIGHT
    g = G(16)
    g.disk(7.5, 7.5, 3.3, 3.3, 'f', 'o')
    for x, y in ((7, 1), (8, 1), (7, 2), (8, 2), (7, 13), (8, 13), (7, 14), (8, 14),
                 (1, 7), (1, 8), (2, 7), (2, 8), (13, 7), (13, 8), (14, 7), (14, 8),
                 (3, 3), (12, 3), (3, 12), (12, 12)):
        g.px(x, y, 'o')
    _ring(g)
    g.px(6, 6, 'w'); g.px(6, 7, 'w')
    return g.rows()


def icon_cake():
    # tarta: nata blanca con goterones, capa rosa, bizcocho crema y una cereza
    g = G(16)
    g.rect(2, 7, 13, 14, 'y')
    g.rect(12, 7, 13, 14, 'Y')
    g.rect(2, 10, 13, 11, 'p'); g.rect(12, 10, 13, 11, 'P')
    g.rect(2, 5, 13, 7, 'w')
    g.px(4, 8, 'w'); g.px(8, 8, 'w'); g.px(8, 9, 'w'); g.px(11, 8, 'w')
    g.disk(7.5, 3, 1.9, 1.9, 'r', 'R')
    g.outline()
    g.px(2, 5, '.'); g.px(13, 5, '.')
    g.px(3, 5, 'k'); g.px(12, 5, 'k')
    g.px(7, 2, 'w')
    g.px(9, 0, 'G'); g.px(8, 1, 'G')
    return g.rows()


def gift():
    # regalo en el suelo de casa: caja blanca, lazo rojo en cruz y moño
    g = G(16)
    g.rect(2, 8, 13, 15, 'w')
    g.rect(12, 8, 13, 15, 's')
    g.rect(1, 5, 14, 8, 'w')
    g.rect(13, 5, 14, 8, 's')
    g.rect(7, 5, 8, 15, 'r')
    g.disk(5, 3, 2.4, 1.8, 'r', 'R')
    g.disk(10, 3, 2.4, 1.8, 'r', 'R')
    g.outline()
    g.rect(2, 8, 13, 8, 'k')                   # borde de la tapa
    g.rect(7, 8, 8, 8, 'R')
    g.rect(7, 3, 8, 4, 'R')                    # nudo
    return g.rows()


def star():
    # estrella amarilla para marcar una baya (escala 1, 16 px): gruesa y llena
    return [
        ".......kk.......",
        "......kffk......",
        "......kwfk......",
        ".....kfwffk.....",
        ".....kffffk.....",
        "kkkkkkffffkkkkkk",
        "kffffffffffffffk",
        ".kffffffffffffk.",
        "..kfffffffffok..",
        "...kffffffffk...",
        "...kfffffffok...",
        "..kffffffffffk..",
        "..kffffkkfffok..",
        ".kfffkk..kkfook.",
        ".kffk......kfok.",
        ".kkk........kkk.",
    ]

SPRITES = {
    "CHARMANDER": charmander(),
    "CHARMELEON": charmeleon(),
    "CHARIZARD": charizard(),
    "BULBASAUR": bulbasaur(),
    "IVYSAUR": ivysaur(),
    "VENUSAUR": venusaur(),
    "SQUIRTLE": squirtle(),
    "WARTORTLE": wartortle(),
    "BLASTOISE": blastoise(),
    "ICON_FOOD": icon_food(),
    "ICON_PLAY": icon_play(),
    "ICON_LIGHT": icon_light(),
    "ICON_CLEAN": icon_clean(),
    "ICON_BERRY_B": icon_berry_blue(),
    "ICON_BERRY_G": icon_berry_green(),
    "ICON_CANDY": icon_candy(),
    "ICON_EGG": icon_egg(),
    "ICON_GOLDEN": icon_golden(),
    "ICON_PROTEIN": icon_protein(),
    "ICON_IRON": icon_iron(),
    "ICON_CARBOS": icon_carbos(),
    "ICON_CHARM": icon_charm(),
    "ICON_RARECANDY": icon_rarecandy(),
    "ICON_FLAG": icon_flag(),
    "ICON_BATTLE": icon_battle(),
    "ICON_SACK": icon_sack(),
    "ICON_DASH": icon_dash(),
    "ICON_ZEN": icon_zen(),
    "ICON_BAG": icon_bag(),
    "ICON_CARE": icon_care(),
    "ICON_SUN": icon_sun(),
    "ICON_CAKE": icon_cake(),
    "EGG": egg(),
    "POOP": poop(),
    "HEART": heart(),
    "GIFT": gift(),
    "STAR": star(),
}

# comentario a medida en la declaracion C (por defecto "NxN") y linea de
# comentario previa: asi los dibujados a mano salen igual que estaban
SPR_NOTE = {
    "ICON_DASH": "16x16: trineo con lineas de velocidad (v1.44)",
    "ICON_ZEN": "16x16: cascada sobre la poza (v1.43)",
}
SPR_PRELUDE = {
    "ICON_EGG": "// huevo de la mochila (v1.39): el de la pantalla principal (SPR_EGG) en pequeno",
}

# anclas (ojos 3x4 / boca) y color de cuerpo para las expresiones superpuestas
ANCHORS = {
    "CHARMANDER": dict(eyeRow=6, eyeL=9, eyeR=17, mouthRow=12, mouthCol=13, body='o'),
    "CHARMELEON": dict(eyeRow=6, eyeL=8, eyeR=16, mouthRow=12, mouthCol=12, body='r'),
    "CHARIZARD":  dict(eyeRow=6, eyeL=10, eyeR=18, mouthRow=12, mouthCol=14, body='o'),
    "BULBASAUR":  dict(eyeRow=10, eyeL=6, eyeR=13, mouthRow=16, mouthCol=9, body='t'),
    "IVYSAUR":    dict(eyeRow=10, eyeL=6, eyeR=13, mouthRow=16, mouthCol=9, body='t'),
    "VENUSAUR":   dict(eyeRow=10, eyeL=5, eyeR=12, mouthRow=16, mouthCol=8, body='t'),
    "SQUIRTLE":   dict(eyeRow=5, eyeL=8, eyeR=16, mouthRow=11, mouthCol=12, body='b'),
    "WARTORTLE":  dict(eyeRow=6, eyeL=7, eyeR=15, mouthRow=12, mouthCol=11, body='l'),
    "BLASTOISE":  dict(eyeRow=6, eyeL=11, eyeR=19, mouthRow=12, mouthCol=15, body='N'),
}


def validate():
    ok = True
    for name, rows in SPRITES.items():
        n = len(rows)
        for i, row in enumerate(rows):
            if len(row) != n:
                print(f"{name} fila {i}: {len(row)} chars (esperaba {n})")
                ok = False
            bad = set(row) - set(PALETTE) - {'.'}
            if bad:
                print(f"{name} fila {i}: chars desconocidos {bad}")
                ok = False
    return ok


def render(path="tools/sheet.png", scale=10, percol=3):
    pad = 16
    names = list(SPRITES)
    ncols = percol
    nrows = (len(names) + ncols - 1) // ncols
    bgs = ['#f2efe1', '#141828']
    cw, chh = W * scale + pad, H * scale + pad + 18
    img = Image.new('RGB', (len(bgs) * (ncols * cw + pad) + pad, nrows * chh + pad), '#888888')
    d = ImageDraw.Draw(img)
    for bi, bg in enumerate(bgs):
        gx = pad + bi * (ncols * cw + pad)
        for i, name in enumerate(names):
            ox = gx + (i % ncols) * cw
            oy = pad + (i // ncols) * chh
            d.rectangle([ox, oy, ox + W * scale, oy + H * scale], fill=bg)
            sc = scale * W // len(SPRITES[name])
            for r, row in enumerate(SPRITES[name]):
                for c, ch in enumerate(row):
                    if ch in PALETTE:
                        d.rectangle([ox + c * sc, oy + r * sc,
                                     ox + (c + 1) * sc - 1, oy + (r + 1) * sc - 1],
                                    fill=PALETTE[ch])
            d.text((ox, oy + H * scale + 3), name, fill='#000')
    img.save(path)
    print(f"guardado {path}")


def rgb565(hexcol):
    r, g, b = int(hexcol[1:3], 16), int(hexcol[3:5], 16), int(hexcol[5:7], 16)
    return (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)


# nombre -> (tipo, evolucionaA, nivelEvolucion, escala)
SPECIES_META = {
    "CHARMANDER": ("TYPE_FUEGO", "SP_CHARMELEON", 16, 5),
    "CHARMELEON": ("TYPE_FUEGO", "SP_CHARIZARD", 36, 6),
    "CHARIZARD":  ("TYPE_FUEGO", "-1", 0, 7),
    "BULBASAUR":  ("TYPE_PLANTA", "SP_IVYSAUR", 16, 5),
    "IVYSAUR":    ("TYPE_PLANTA", "SP_VENUSAUR", 36, 6),
    "VENUSAUR":   ("TYPE_PLANTA", "-1", 0, 7),
    "SQUIRTLE":   ("TYPE_AGUA", "SP_WARTORTLE", 16, 5),
    "WARTORTLE":  ("TYPE_AGUA", "SP_BLASTOISE", 36, 6),
    "BLASTOISE":  ("TYPE_AGUA", "-1", 0, 7),
}

ACCENT = {"TYPE_FUEGO": '#e8503a', "TYPE_PLANTA": '#3c8a4c', "TYPE_AGUA": '#4f93c4'}

UI_COLORS = {
    'UI_BG_DAY': '#f2efe1', 'UI_BG_NIGHT': '#141828',
    'UI_INK': '#2a2a36', 'UI_INK_NIGHT': '#d8dcf0',
    'UI_TRACK': '#d8d2bd',
    'UI_BAR_OK': '#58b868', 'UI_BAR_WARN': '#e8a23c', 'UI_BAR_BAD': '#e8503a',
    'UI_WHITE': '#ffffff',
}


def emit_c(path="species.h"):
    out = []
    out.append("#pragma once\n#include <stdint.h>\n\n")
    out.append("// GENERADO por tools/sprites.py - edita alli y ejecuta:\n")
    out.append("//   python3 tools/sprites.py emit\n\n")
    out.append(f"#define SPRITE_W {W}\n#define SPRITE_H {H}\n\n")
    for name, hexcol in UI_COLORS.items():
        out.append(f"#define {name} 0x{rgb565(hexcol):04X}  // {hexcol}\n")
    out.append("\nenum ElementType : uint8_t { TYPE_FUEGO, TYPE_PLANTA, TYPE_AGUA };\n\n")
    ids = list(SPECIES_META)
    out.append("enum : int8_t {\n")
    for i, name in enumerate(ids):
        out.append(f"  SP_{name}{' = 0' if i == 0 else ''},\n")
    out.append("  NUM_SPECIES\n};\n\n")
    out.append(
        "struct Species {\n"
        "  const char *name;\n"
        "  ElementType type;\n"
        "  const char *const *sprite;\n"
        "  uint8_t scale;\n"
        "  int8_t evolvesTo;\n"
        "  uint8_t evolveLevel;\n"
        "  uint8_t eyeRow, eyeColL, eyeColR;  // anclas para expresiones (ojos 3x4)\n"
        "  uint8_t mouthRow, mouthCol;\n"
        "  uint16_t bodyColor;  // para borrar ojos/boca al expresar\n"
        "  uint16_t accent;     // color UI del tipo\n"
        "};\n\n")
    out.append("// caracter de sprite -> RGB565\n")
    out.append("static inline uint16_t spriteColor(char ch) {\n  switch (ch) {\n")
    for ch, hexcol in PALETTE.items():
        out.append(f"    case '{ch}': return 0x{rgb565(hexcol):04X};  // {hexcol}\n")
    out.append("    default: return 0;\n  }\n}\n\n")
    for name, rows in SPRITES.items():
        if name in SPR_PRELUDE:
            out.append(SPR_PRELUDE[name] + "\n")
        note = SPR_NOTE.get(name, f"{len(rows)}x{len(rows)}")
        out.append(f"static const char* const SPR_{name}[{len(rows)}] = {{  // {note}\n")
        for row in rows:
            out.append(f'  "{row}",\n')
        out.append("};\n\n")
    out.append("static const Species SPECIES[NUM_SPECIES] = {\n")
    for name in ids:
        typ, evo, lvl, scale = SPECIES_META[name]
        a = ANCHORS[name]
        body = rgb565(PALETTE[a['body']])
        acc = rgb565(ACCENT[typ])
        out.append(
            f'  {{ "{name}", {typ}, SPR_{name}, {scale}, {evo}, {lvl}, '
            f"{a['eyeRow']}, {a['eyeL']}, {a['eyeR']}, {a['mouthRow']}, {a['mouthCol']}, "
            f"0x{body:04X}, 0x{acc:04X} }},\n")
    out.append("};\n\n")
    out.append("static const int8_t STARTERS[] = { SP_CHARMANDER, SP_BULBASAUR, SP_SQUIRTLE };\n")
    out.append("#define NUM_STARTERS 3\n")
    open(path, 'w').write(''.join(out))
    print(f"guardado {path}")


def emit_js(path="tools/emitted_sprites.js"):
    import json
    meta = {}
    ids = list(SPECIES_META)
    for name in ids:
        typ, evo, lvl, scale = SPECIES_META[name]
        a = ANCHORS[name]
        meta[name] = {
            'accent': ACCENT[typ],
            'evolvesTo': None if evo == '-1' else ids.index(evo[3:]),
            'evolveLevel': lvl,
            'scale': scale,
            'eyeRow': a['eyeRow'], 'eyeL': a['eyeL'], 'eyeR': a['eyeR'],
            'mouthRow': a['mouthRow'], 'mouthCol': a['mouthCol'],
            'body': PALETTE[a['body']],
        }
    data = {'palette': PALETTE, 'sprites': SPRITES, 'meta': meta, 'order': ids}
    open(path, 'w').write("const SPRITE_DATA=" + json.dumps(data, separators=(',', ':')) + ";\n")
    print(f"guardado {path}")


if __name__ == '__main__':
    import sys
    if not validate():
        print("-- errores de validacion --")
        sys.exit(1)
    render()
    if 'emit' in sys.argv:
        emit_c()
        emit_js()
