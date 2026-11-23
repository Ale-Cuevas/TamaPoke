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
    'v': '#efe68a',  # amarillo palido (cristal del Revivir, v1.62)
    'V': '#b4a640',  # oliva (sombra del cristal)
    'u': '#a672d6',  # lila (insignia fantasma)
    'U': '#6c42a6',  # lila sombra
    'q': '#c2489e',  # magenta (insignia veneno)
    'Q': '#80296c',  # magenta sombra
    'i': '#4f5bc2',  # indigo (insignia dragon)
    'I': '#2f3586',  # indigo sombra
    # v3.20: los muebles de casa (camas, gimnasio, juguete)
    'h': '#e8c860',  # paja / cuerda
    'H': '#b08c38',  # paja sombra
    'a': '#dcac72',  # madera clara (cortes de tronco, asiento)
    'j': '#f2c230',  # oro (cama real)
    'J': '#b8860b',  # oro sombra
    'x': '#8a4cc0',  # purpura real (colchon)
    'X': '#5a2c88',  # purpura sombra
    'e': '#4a4e5a',  # hierro (pesas)
    # v3.22: sombra gris claro de las particulas de tipo (PART_*), que el patio tinta
    'z': '#d4d9e2',
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

def icon_berry_red():
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


def _berry16(body, shade, leaf='g'):
    g = G(16)
    g.disk(8, 9.5, 5.2, 4.8, body, shade)
    g.px(8, 3, 'C'); g.px(8, 4, 'C')
    g.px(10, 3, leaf); g.px(11, 2, leaf)
    g.outline()
    g.px(6, 7, 'w'); g.px(5, 8, 'w')
    return g.rows()


def icon_berry_plain():
    # la baya normal (v3.2, gratis): lila, distinta de las seis de colores y de la dorada
    return _berry16('u', 'U')


def icon_berry_yellow():
    return _berry16('v', 'V')


def icon_berry_pink():
    return _berry16('p', 'P')


def icon_berry_orange():
    return _berry16('o', 'O')


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


# ---- v3.22 PALS: iconos que flotan sobre los bichos (16x16) y particulas de tipo (8x8, blancas, se tintan)

def icon_note():
    # corchea rosa
    g = G(16)
    g.disk(5.5, 11.5, 3.2, 2.7, 'p', 'P')
    g.rect(7, 2, 9, 11, 'p')
    g.rect(9, 2, 13, 4, 'p'); g.rect(11, 4, 13, 7, 'p')
    g.px(13, 8, 'p')
    g.outline()
    g.px(4, 10, 'w'); g.px(3, 11, 'w')
    return g.rows()


def icon_dots():
    # globo de dialogo con "..."
    g = G(16)
    g.disk(7.5, 6.5, 7.0, 5.0, 'w', 's')
    g.rect(3, 10, 5, 13, 'w'); g.rect(3, 13, 3, 14, 'w')
    g.outline()
    for x in (4, 7, 10):
        g.rect(x, 6, x + 1, 7, 'S')
    return g.rows()


def icon_pout():
    # nubecita de vapor con la marca roja de enfado
    g = G(16)
    g.disk(4.5, 9.5, 3.6, 3.6, 'w', 's')
    g.disk(9.0, 7.0, 4.4, 4.4, 'w', 's')
    g.disk(11.5, 10.5, 3.2, 3.2, 'w', 's')
    g.rect(4, 10, 12, 13, 'w')
    g.outline()
    # marca de enfado: cuatro trazos en cruz
    for x, y in ((7, 5), (8, 5), (6, 6), (9, 6), (6, 8), (9, 8), (7, 9), (8, 9)):
        g.px(x, y, 'r')
    g.px(7, 7, 'R'); g.px(8, 7, 'R')
    return g.rows()


def icon_grumpy():
    # nube gris oscura con un rayo
    g = G(16)
    g.disk(4.5, 7.5, 3.6, 3.6, 's', 'S')
    g.disk(9.0, 5.5, 4.6, 4.2, 's', 'S')
    g.disk(11.5, 8.0, 3.2, 3.2, 's', 'S')
    g.rect(4, 8, 12, 10, 'S')
    g.outline()
    for x, y in ((9, 10), (8, 10), (7, 11), (8, 11), (6, 12), (7, 12), (8, 12), (9, 12),
                 (8, 13), (7, 13), (7, 14), (6, 15)):
        g.px(x, y, 'f')
    g.px(9, 11, 'k'); g.px(10, 11, 'k'); g.px(10, 12, 'k'); g.px(9, 13, 'k'); g.px(8, 14, 'k'); g.px(7, 15, 'k')
    return g.rows()


def icon_spark():
    # chispa de cuatro puntas
    g = G(16)
    for i in range(1, 8):
        w = max(0, (8 - i) // 3)
        g.rect(7 - w, 7 - i, 8 + w, 7 - i, 'f')
        g.rect(7 - w, 8 + i, 8 + w, 8 + i, 'f')
        g.rect(7 - i, 7 - w, 7 - i, 8 + w, 'f')
        g.rect(8 + i, 7 - w, 8 + i, 8 + w, 'f')
    g.rect(5, 5, 10, 10, 'f')
    g.rect(6, 6, 9, 9, 'w')
    g.outline()
    return g.rows()


# particulas de tipo: 8x8, solo 'w' y 'z' (sin contorno) para teñirlas de un color liso
PART_FLAME = [
    "...w....",
    "..ww....",
    "..www.w.",
    ".wwwwww.",
    ".wwwwww.",
    ".wwzwww.",
    "..wzzw..",
    "...ww...",
]
PART_DROP = [
    "...ww...",
    "...ww...",
    "..wwww..",
    "..wwww..",
    ".wwwwww.",
    ".wwwwzz.",
    "..wzzz..",
    "...zz...",
]
PART_LEAF = [
    "......ww",
    "....wwww",
    "..wwwwww",
    ".wwwwwww",
    ".wwwwwwz",
    ".wwwwzz.",
    "wwzzz...",
    "w.......",
]
PART_BOLT = [
    "....www.",
    "...www..",
    "..www...",
    ".wwwwww.",
    "...wwz..",
    "..wwz...",
    "..wz....",
    ".wz.....",
]
PART_STAR = [
    "...ww...",
    "...ww...",
    "...ww...",
    "wwwwwwww",
    "wwwwwwzz",
    "...zz...",
    "...zz...",
    "...zz...",
]


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


def icon_pet():
    # corazon del mimo (v2.3): se anadio a species.h a mano; fijado tal cual
    return [
        "................",
        "................",
        "................",
        "...kkk....kkk...",
        "..krrrk..krrrk..",
        ".krwwrrkkrrrrRk.",
        ".krwrrrrrrrrrRk.",
        ".krrrrrrrrrrrRk.",
        ".krrrrrrrrrrrRk.",
        "..krrrrrrrrrRk..",
        "...krrrrrrrRk...",
        "....krrrrrRk....",
        ".....krrrRk.....",
        "......krRk......",
        ".......kk.......",
        "................",
    ]


def icon_shop():
    # tienda (v3.3, el boton del dock): una moneda dorada con su canto y una ranura, como la de la cartera
    g = G(16)
    g.disk(7.5, 7.5, 6.4, 6.4, 'f', 'Y')
    g.outline()
    g.rect(7, 4, 8, 11, 'O')
    g.px(5, 4, 'w'); g.px(4, 5, 'w'); g.px(4, 6, 'w')
    return g.rows()


def icon_card():
    # la ficha del bicho (v3.3, "Estado" en su menu): una tarjeta con un retrato y dos barras
    g = G(16)
    g.rect(1, 3, 14, 13, 'w')
    g.outline()
    g.rect(3, 5, 6, 8, 'b'); g.px(6, 8, 'B'); g.px(6, 7, 'B'); g.px(5, 8, 'B')
    g.rect(8, 5, 12, 5, 's')
    g.rect(8, 7, 11, 7, 's')
    g.rect(3, 10, 12, 10, 'g'); g.rect(10, 10, 12, 10, 'S')
    g.rect(3, 11, 8, 11, 'o'); g.rect(9, 11, 12, 11, 'S')
    return g.rows()


def icon_letter():
    # una carta (v3.3, "Despedirse" en el menu del bicho: quiere decirte algo), cerrada con un corazon
    g = G(16)
    g.rect(1, 4, 14, 12, 'y')
    g.outline()
    for i in range(4):  # la solapa: dos diagonales que bajan hasta el corazon
        g.px(2 + i, 5 + i, 'Y')
        g.px(13 - i, 5 + i, 'Y')
    g.rect(6, 8, 9, 9, 'r'); g.px(6, 8, 'r'); g.px(9, 8, 'r')
    g.px(7, 10, 'r'); g.px(8, 10, 'r')
    g.px(7, 8, 'R'); g.px(8, 8, 'R')
    return g.rows()


def icon_revive():
    # Revivir (v1.62): un cristal amarillo inclinado (era una pluma dorada), con el borde de
    # arriba en escalera, un brillo blanco en triangulo arriba y la sombra oliva abajo a la derecha.
    # No es un corazon: el boton de Cuidar ya lo es
    return [
        "............k...",
        "..........kkvk..",
        "........kkwwvk..",
        "......kkwwwvvk..",
        "....kkwwwwvvvk..",
        "...kwwwwwwvvvk..",
        "...kwwwwwvvvVk..",
        "...kvwwwvvVVVk..",
        "...kvwwwvVVVVk..",
        "...kvvvvvVVVVk..",
        "...kvvvvVVVVVk..",
        "...kVvvvVVVVk...",
        "...kVVVVVVkk....",
        "...kVVVVkk......",
        "...kVVkk........",
        "....kk..........",
    ]


def icon_lure():
    # Cebo (v3.14): un pez de cebo naranja colgado de su sedal, con la tripa amarilla, un ojo y el anzuelo
    # debajo. Hace salir a un salvaje (no es la Poke Ball: nada de rojo y blanco)
    g = G(16)
    g.disk(9, 8, 5.4, 3.9, 'o', 'O')       # cuerpo
    for i in range(4):                     # cola: un abanico que se abre hacia la izquierda
        g.rect(3 - i, 7 - i, 3 - i, 9 + i, 'o')
    g.rect(6, 10, 11, 10, 'f')             # tripa
    g.outline()
    g.rect(1, 6, 1, 10, 'O'); g.rect(2, 7, 2, 9, 'O')  # la cola, en sombra
    g.rect(11, 6, 12, 7, 'k'); g.px(11, 6, 'w')  # ojo con brillo
    g.px(7, 6, 'w'); g.px(6, 7, 'w')       # brillo del lomo
    g.rect(14, 0, 14, 6, 'S')              # sedal
    g.rect(9, 12, 9, 14, 'S'); g.px(8, 15, 'S'); g.px(7, 15, 'S'); g.px(6, 14, 'S')  # anzuelo
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


def wild_bush():
    # encuentro salvaje (v2.2): el arbusto que se mueve en el prado y el bosque. Tres matas verdes con brillo
    g = G()
    g.disk(9.5, 19, 8, 7.5, 'g', 'G')
    g.disk(22.5, 19, 8, 7.5, 'g', 'G')
    g.disk(16, 13, 9, 8.5, 'g', 'G')
    g.disk(16, 22, 11, 6.5, 'g', 'G')
    g.rect(5, 26, 27, 28, 'G')
    g.outline()
    for x, y in ((11, 8), (12, 9), (13, 7), (5, 15), (6, 14), (19, 9), (25, 14), (26, 15), (14, 17), (21, 19)):
        g.px(x, y, 't')
    for x, y in ((9, 22), (17, 24), (23, 23), (13, 26), (20, 27)):
        g.px(x, y, 'd')
    return g.rows()


def wild_rock():
    # encuentro salvaje (v2.2): la roca de la cantera y las ruinas, gris con una grieta
    g = G()
    g.disk(16, 20, 13.5, 9.5, 's', 'S')
    g.disk(13, 13, 8, 7, 's', 'S')
    g.rect(3, 26, 29, 28, 'S')
    g.outline()
    for x, y in ((10, 9), (11, 8), (12, 8), (9, 10), (20, 15), (21, 15), (6, 18)):
        g.px(x, y, 'w')
    for x, y in ((18, 18), (19, 19), (19, 20), (20, 21), (21, 22), (21, 23), (15, 12), (16, 13)):
        g.px(x, y, 'k')
    return g.rows()


def wild_sand():
    # encuentro salvaje (v2.2): el monton de arena de la playa, con chinas y una concha
    g = G()
    g.disk(16, 25, 14.5, 7.5, 'y', 'Y')
    g.disk(14, 20, 8, 5.5, 'y', 'Y')
    g.rect(3, 28, 29, 29, 'Y')
    g.outline()
    for x, y in ((9, 22), (21, 25), (14, 27), (25, 22)):
        g.px(x, y, 'c')
    g.px(11, 17, 'w'); g.px(12, 16, 'w'); g.px(6, 23, 'w')
    g.rect(18, 19, 20, 20, 'p'); g.px(19, 18, 'p')  # concha
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

def badge_grass():
    # insignia de fase (hoja verde), 16x16 con contorno y brillo
    return [
        "................",
        "..........kkkk..",
        "......kkkkggwgk.",
        "....kkggggggggk.",
        "...kgwgggggggGk.",
        "..kgwggkgggggGk.",
        "..kggggGkggggGk.",
        "..kgggggGkgggGk.",
        "..kggggggGkggGk.",
        "..kgggggggGkGGk.",
        "..kGggggggGGGk..",
        "...kGGggGGGGkk..",
        "....kkGGGkkk....",
        "....kk.kk.......",
        "...kk...........",
        "..kk............",
    ]

def badge_water():
    # insignia de fase (gota azul), 16x16 con contorno y brillo
    return [
        ".......kk.......",
        "......kbbk......",
        "......kbbk......",
        ".....kbbbBk.....",
        ".....kbwbBk.....",
        "....kbbwbbBk....",
        "...kbbwbbbBBk...",
        "...kbwbbbbBBk...",
        "..kbbwbbbbbBBk..",
        "..kbbbbbbbbBBk..",
        "..kbbbbbbbBBBk..",
        "..kBbbbbbBBBBk..",
        "...kBBbbBBBBk...",
        "...kkBBBBBBkk...",
        ".....kkkkkk.....",
        "................",
    ]

def badge_rock():
    # insignia de fase (roca marron facetada), 16x16 con contorno y brillo
    return [
        "................",
        "......kk........",
        "....kkssk.......",
        "...ksswcckkk....",
        "..kswccccccckk..",
        ".kswcccckcccccck",
        ".kwccccckkcccCCk",
        "kccckcccckccCCCk",
        "kcccckkcccccCCCk",
        "kccccccckccCCCk.",
        ".kcccccckkCCCCk.",
        ".kcccccCCCCCCk..",
        "..kcccCCCCCkk...",
        "...kkCCCCkk.....",
        ".....kkkk.......",
        "................",
    ]

def badge_fire():
    # insignia de fase (llama roja y naranja), 16x16 con contorno y brillo
    return [
        ".......k........",
        "......krk.......",
        "......krrk......",
        ".....krrrk..k...",
        ".....krorrk.kk..",
        "....krooorrkkrk.",
        "...krrooofrrrrk.",
        "...krooofffrrRk.",
        "..krooofffforrRk",
        "..kroofffwffoRRk",
        "..kroofffffforRk",
        "..kroooffffooRRk",
        "...krooofffooRk.",
        "...kRooooooRRk..",
        "....kkRRRRRkk...",
        "......kkkkk.....",
    ]

def badge_electric():
    # insignia de fase (rayo amarillo), 16x16 con contorno y brillo
    return [
        "................",
        "......kkkkkk....",
        ".....kffwffk....",
        ".....kfwffYk....",
        "....kffffYYk....",
        "....kfffYYk.....",
        "...kffffYYkkkk..",
        "...kfffffffffYk.",
        "...kkkkkfffYYk..",
        "......kfffYYk...",
        ".....kffYYYk....",
        ".....kfYYYk.....",
        "....kfYYkk......",
        "....kYkk........",
        "....kk..........",
        "................",
    ]

def badge_ghost():
    # insignia de fase (fantasma lila), 16x16 con contorno y brillo
    return [
        "................",
        ".....kkkkkk.....",
        "....kuuuuuuk....",
        "...kuwwuuuuUk...",
        "..kuwuuuuuuuUk..",
        "..kuuukuukuuUk..",
        "..kuuukuukuuUk..",
        "..kuuuukkuuuUk..",
        "..kuuuuuuuuuUk..",
        "..kuuuuuuuuUUk..",
        "..kuuuuuuuuUUk..",
        "..kuuUuuuUuUUk..",
        "..kuUkUuUkUkUk..",
        "..kUk.kUk.kUk...",
        "...k...k...k....",
        "................",
    ]

def badge_poison():
    # insignia de fase (frasco de veneno magenta), 16x16 con contorno y brillo
    return [
        "................",
        "......kkkk......",
        "......kwQk......",
        "......kqQk......",
        "......kqQk......",
        ".....kkqQkk.....",
        "....kqqqqQQk....",
        "...kqqwqqqQQk...",
        "..kqqwqqgqqQQk..",
        "..kqqqqqGgqQQk..",
        "..kqqqqqqGqQQk..",
        "..kqgqqqqqqQQk..",
        "..kqGgqqqqQQQk..",
        "...kQQQQQQQQk...",
        "....kkkkkkkk....",
        "................",
    ]

def badge_dragon():
    # insignia de fase (colmillo indigo con borde dorado), 16x16 con contorno y brillo
    return [
        "................",
        "..kkkkkkkkkkk...",
        ".kffffffffffYk..",
        ".kYYYYYYYYYYYk..",
        "..kiiiiiiiiIIk..",
        "..kiwwiiiiiIIk..",
        "..kiwiiiiiiIIk..",
        "..kiiiiiiiiIk...",
        "...kiiiiiiIIk...",
        "...kiiiiiiIk....",
        "....kiiiiIIk....",
        "....kiiiiIk.....",
        ".....kiiIk......",
        "......kiIk......",
        ".......kk.......",
        "................",
    ]


# ---------------------------------------------------------------- muebles de casa (v3.20, FR-HOME)
# 32x32 a escala 2 en el patio, con los pies (la fila 26) en el centro de su celda; las camas, con el
# colchon en las filas 14-28 para que el bicho tumbado quede encima. Tres niveles por linea.

def bed1():
    # cama de paja: un nido de paja con briznas sueltas y una almohada de paja mas clara
    g = G()
    g.disk(16, 21, 14.5, 7.5, 'h', 'H')
    g.disk(9, 16, 5.5, 3.2, 'h', 'H')
    g.outline()
    for x, y in ((6, 21), (7, 22), (12, 24), (13, 25), (19, 19), (20, 20), (24, 23), (25, 24), (16, 26),
                 (22, 17), (27, 20), (4, 23), (10, 19), (18, 23)):
        g.px(x, y, 'H')
    for x, y in ((8, 15), (9, 15), (15, 17), (21, 22), (11, 21)):
        g.px(x, y, 'w')
    g.px(1, 18, 'h'); g.px(30, 24, 'h'); g.px(29, 16, 'H')  # briznas que se salen
    return g.rows()


def bed2():
    # cama de cojin: marco de madera con cabecero, colchon rosa y almohada blanca
    g = G()
    g.rect(2, 9, 29, 14, 'c')            # cabecero
    g.rect(2, 14, 29, 29, 'c')           # marco
    g.rect(26, 14, 29, 29, 'C')
    g.rect(4, 15, 27, 27, 'p')           # colchon
    g.rect(22, 15, 27, 27, 'P')
    g.disk(10, 16.5, 5, 2.8, 'w', 's')    # almohada
    g.rect(4, 22, 27, 23, 'P')           # pliegue de la manta
    g.outline()
    g.rect(3, 10, 28, 10, 'C')           # veta del cabecero
    g.px(2, 29, 'C'); g.px(29, 29, 'C')
    return g.rows()


def bed3():
    # cama real: marco de oro con postes y pomos, colchon purpura, almohada roja y una corona en el cabecero
    g = G()
    g.rect(2, 9, 29, 14, 'j')            # cabecero
    g.rect(2, 14, 29, 29, 'j')           # marco
    g.rect(26, 9, 29, 29, 'J')
    g.disk(3, 6, 2.2, 2.2, 'j', 'J')     # pomos
    g.disk(28, 6, 2.2, 2.2, 'j', 'J')
    g.rect(13, 4, 18, 8, 'j')            # corona
    g.px(13, 3, 'j'); g.px(15, 2, 'j'); g.px(16, 2, 'j'); g.px(18, 3, 'j')
    g.rect(4, 15, 27, 27, 'x')           # colchon
    g.rect(22, 15, 27, 27, 'X')
    g.disk(10, 16.5, 5, 2.8, 'r', 'R')    # almohada
    g.rect(4, 22, 27, 23, 'X')           # borde de la colcha
    g.outline()
    g.px(15, 6, 'r'); g.px(16, 6, 'r')   # la joya de la corona
    for x in range(5, 27, 4):
        g.px(x, 23, 'j')                 # flecos de oro
    g.px(8, 15, 'w'); g.px(9, 15, 'w')
    return g.rows()


def gym1():
    # pesas de tronco: dos rodajas de tronco de pie unidas por un palo
    g = G()
    g.rect(9, 17, 22, 20, 'C')           # el palo
    for cx in (6, 25):
        g.rect(cx - 4, 13, cx + 4, 26, 'c')
        g.rect(cx + 3, 13, cx + 4, 26, 'C')
        g.disk(cx, 26, 4.6, 2.4, 'c', 'C')
        g.disk(cx, 13, 4.6, 2.4, 'a')    # el corte de arriba
    g.outline()
    for cx in (6, 25):
        g.px(cx - 2, 13, 'c'); g.px(cx + 2, 13, 'c'); g.px(cx, 12, 'c'); g.px(cx, 14, 'c')  # el anillo
        g.px(cx - 2, 19, 'C'); g.px(cx - 1, 22, 'C')                                       # corteza
    g.rect(10, 17, 21, 17, 'c')          # brillo del palo
    return g.rows()


def gym2():
    # mancuernas y saco de boxeo: un poste con brazo del que cuelga el saco rojo, y una mancuerna en el suelo
    g = G()
    g.rect(26, 2, 28, 29, 's')           # poste
    g.rect(28, 2, 28, 29, 'S')
    g.rect(14, 2, 28, 4, 's')            # brazo
    g.rect(23, 28, 31, 30, 'S')          # pie
    g.disk(16, 14, 5.5, 8.5, 'r', 'R')   # saco
    g.rect(11, 8, 21, 8, 'R')
    g.rect(15, 5, 16, 6, 'S')            # cadena
    g.rect(2, 26, 14, 27, 'S')           # mancuerna
    g.rect(2, 23, 5, 30, 'e')
    g.rect(11, 23, 14, 30, 'e')
    g.outline()
    g.rect(12, 11, 13, 16, 'w')          # brillo del saco
    g.px(3, 24, 's'); g.px(12, 24, 's')
    return g.rows()


def gym3():
    # gimnasio completo: banco con barra de discos grandes, saco colgado y una colchoneta azul debajo
    g = G()
    g.rect(0, 24, 31, 30, 'b')           # colchoneta
    g.rect(0, 29, 31, 30, 'B')
    g.rect(1, 2, 2, 27, 's')             # el marco: dos postes y la barra de arriba
    g.rect(29, 2, 30, 27, 'S')
    g.rect(1, 2, 30, 3, 's')
    g.disk(23, 12, 4.5, 7.5, 'r', 'R')   # saco
    g.rect(23, 4, 23, 5, 'S')
    g.rect(5, 18, 18, 21, 'c')           # banco
    g.rect(6, 22, 7, 26, 'C'); g.rect(16, 22, 17, 26, 'C')
    g.rect(3, 14, 20, 15, 'S')           # barra con discos
    g.rect(3, 9, 5, 20, 'e')
    g.rect(18, 9, 20, 20, 'e')
    g.outline()
    g.rect(21, 9, 21, 13, 'w')           # brillo del saco
    g.px(4, 10, 's'); g.px(19, 10, 's')
    for x in range(3, 30, 6):
        g.px(x, 27, 'B')                 # costuras de la colchoneta
    return g.rows()


def toy1():
    # pelota de goma: roja y blanca, con brillo
    g = G()
    g.disk(16, 18, 8.5, 8.5, 'r', 'R')
    g.rect(7, 17, 25, 19, 'w')           # franja blanca
    g.outline()
    g.rect(8, 18, 24, 18, 's')
    g.px(12, 12, 'w'); g.px(13, 11, 'w'); g.px(12, 13, 'w')
    return g.rows()


def toy2():
    # pelota y cuerda de tirar: la pelota a la izquierda y una cuerda con nudos enroscada a la derecha
    g = G()
    g.disk(10, 20, 7, 7, 'r', 'R')
    g.rect(3, 19, 17, 21, 'w')
    g.disk(23, 24, 7.5, 4.5, 'h', 'H')   # el rollo de cuerda
    g.erase(23, 24, 3.6, 1.8)
    g.disk(28, 17, 2.4, 2.4, 'h', 'H')   # un nudo que se sale
    g.rect(27, 19, 28, 21, 'h')
    g.outline()
    g.rect(3, 20, 17, 20, 's')
    g.px(7, 15, 'w'); g.px(8, 14, 'w')
    for x, y in ((18, 23), (21, 21), (25, 21), (28, 24), (24, 27), (19, 26)):
        g.px(x, y, 'H')
    return g.rows()


def toy3():
    # columpio: la barra de arriba sobre dos patas en A, dos cuerdas y el asiento
    g = G()

    def leg(x0, y0, x1, y1, ch):
        n = max(abs(x1 - x0), abs(y1 - y0))
        for i in range(n + 1):
            x = round(x0 + (x1 - x0) * i / n)
            y = round(y0 + (y1 - y0) * i / n)
            g.rect(x, y, x + 2, y, ch)

    leg(4, 4, 1, 29, 'c'); leg(4, 4, 8, 29, 'C')
    leg(26, 4, 22, 29, 'c'); leg(26, 4, 29, 29, 'C')
    g.rect(2, 3, 29, 5, 'c')             # la barra de arriba
    g.rect(2, 5, 29, 5, 'C')
    g.rect(11, 6, 11, 21, 'H')           # cuerdas
    g.rect(20, 6, 20, 21, 'H')
    g.rect(9, 21, 22, 23, 'a')           # asiento
    g.rect(9, 23, 22, 23, 'C')
    g.outline()
    g.rect(3, 4, 28, 4, 'a')
    return g.rows()


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
    "ICON_BERRY": icon_berry_plain(),
    "ICON_BERRY_R": icon_berry_red(),
    "ICON_PLAY": icon_play(),
    "ICON_LIGHT": icon_light(),
    "ICON_BERRY_B": icon_berry_blue(),
    "ICON_BERRY_G": icon_berry_green(),
    "ICON_BERRY_Y": icon_berry_yellow(),
    "ICON_BERRY_P": icon_berry_pink(),
    "ICON_BERRY_O": icon_berry_orange(),
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
    "ICON_SUN": icon_sun(),
    "ICON_NOTE": icon_note(),
    "ICON_DOTS": icon_dots(),
    "ICON_POUT": icon_pout(),
    "ICON_GRUMPY": icon_grumpy(),
    "ICON_SPARK": icon_spark(),
    "PART_FLAME": PART_FLAME,
    "PART_DROP": PART_DROP,
    "PART_LEAF": PART_LEAF,
    "PART_BOLT": PART_BOLT,
    "PART_STAR": PART_STAR,
    "ICON_CAKE": icon_cake(),
    "ICON_REVIVE": icon_revive(),
    "ICON_PET": icon_pet(),
    "ICON_SHOP": icon_shop(),
    "ICON_CARD": icon_card(),
    "ICON_LETTER": icon_letter(),
    "ICON_LURE": icon_lure(),
    "BADGE_GRASS": badge_grass(),
    "BADGE_WATER": badge_water(),
    "BADGE_ROCK": badge_rock(),
    "BADGE_FIRE": badge_fire(),
    "BADGE_ELECTRIC": badge_electric(),
    "BADGE_GHOST": badge_ghost(),
    "BADGE_POISON": badge_poison(),
    "BADGE_DRAGON": badge_dragon(),
    "EGG": egg(),
    "POOP": poop(),
    "HEART": heart(),
    "GIFT": gift(),
    "STAR": star(),
    "WILD_BUSH": wild_bush(),
    "WILD_ROCK": wild_rock(),
    "WILD_SAND": wild_sand(),
    "BED1": bed1(),
    "BED2": bed2(),
    "BED3": bed3(),
    "GYM1": gym1(),
    "GYM2": gym2(),
    "GYM3": gym3(),
    "TOY1": toy1(),
    "TOY2": toy2(),
    "TOY3": toy3(),
}

# comentario a medida en la declaracion C (por defecto "NxN") y linea de
# comentario previa: asi los dibujados a mano salen igual que estaban
SPR_NOTE = {
    "ICON_DASH": "16x16: trineo con lineas de velocidad (v1.44)",
    "ICON_ZEN": "16x16: cascada sobre la poza (v1.43)",
    "ICON_PET": "16x16: corazon, el mimo del panel Cuidar (v2.3)",
    "ICON_SHOP": "16x16: moneda, el boton Tienda del dock (v3.3)",
    "ICON_CARD": "16x16: ficha, Estado en el menu del bicho (v3.3)",
    "ICON_LETTER": "16x16: carta, Despedirse en el menu del bicho (v3.3)",
    "ICON_LURE": "16x16: pez de cebo con sedal y anzuelo, el Cebo de la mochila (v3.14)",
    "BED1": "32x32: cama de paja (v3.20, mejoras de casa nivel 1)",
    "BED2": "32x32: cama de cojin (v3.20, nivel 2)",
    "BED3": "32x32: cama real (v3.20, nivel 3)",
    "GYM1": "32x32: pesas de tronco (v3.20, gimnasio nivel 1)",
    "GYM2": "32x32: mancuerna y saco de boxeo (v3.20, nivel 2)",
    "GYM3": "32x32: gimnasio completo (v3.20, nivel 3)",
    "TOY1": "32x32: pelota de goma (v3.20, juguete nivel 1)",
    "TOY2": "32x32: pelota y cuerda de tirar (v3.20, nivel 2)",
    "TOY3": "32x32: columpio (v3.20, nivel 3)",
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
