#!/usr/bin/env python3
"""Fondos del patio (v2.0): mapas de texto + paquete de baldosas -> ficheros TPY1 para la SD.

Lee tools/yard/maps/NN-nombre.txt y su leyenda (tools/yard/legends/), compone una imagen de
240x240 con baldosas de 16 px y escribe en tools/yard/out/:
  NN-nombre.bin           fichero TPY1 (paleta + mascara de paso + pixeles indexados)
  NN-nombre_preview.png   466x466 redondo (x2,4 del centro, recorte de 7, mascara, HUD de muestra)
  NN-nombre_debug.png     lo mismo con la rejilla, las celdas bloqueadas y las zonas del HUD

Ambiente opcional en la cabecera del mapa: "hue: A-B T [keep|dark|murk]" (puede repetirse; se aplican en orden;
"keep" conserva saturacion y valor, "dark" conserva la saturacion y oscurece, "murk" deja un liquido turbio: sat 55 %, valor 30 %), "tint: RRGGBB N" (mezcla N %) y "mist: N [RRGGBB]".

Mas opciones (v3.15, fondos nuevos):
  sky: RRGGBB RRGGBB      degradado vertical (arriba, abajo) pintado bajo todo. Un caracter del suelo con
                          la leyenda "o = void !" no pinta baldosa: se ve el cielo y bloquea.
  decal:                  tercera capa opcional (rejilla de 15x15 tras "objects:"), dibujada encima de los
                          objetos. En la leyenda: "p = draw <nombre> [WxH] [!]". Los pintores son pixel art
                          procedural (sin arte con licencia): lava_crack, bolt, pylon (2x3), smokestack (2x3),
                          cloud_puff (2x1), vent, bubbles, floor_crack, compass (3x3) y banner (1x2). Mismas comprobaciones de solape y de limite que los stamps.

  uv run --no-project --with pillow tools/pack_yard.py          # todos los mapas
  uv run --no-project --with pillow tools/pack_yard.py 01 03    # solo esos
  uv run --no-project --with pillow tools/pack_yard.py --no-preview

El paquete de baldosas NO esta en git (su licencia prohibe redistribuirlo): cada uno lo
descarga y lo descomprime en tools/yard/tilesets/<pack>/ (ver la leyenda). Los .bin llevan
el arte, asi que tampoco se suben: solo van a la SD (tools/send_sd.py --yard).

Formato TPY1 (docs/yard-prototype/DESIGN_YARD_UI.md, 4.1):
  "TPY1" | u8 version=1 | u8 w=240 | u8 h=240 | u8 palCount (0 = 256)
  | u16 pal[palCount] RGB565 little-endian | u8 walk[29] (15x15 bits, LSB primero, 1 = bloqueada)
  | u8 px[w*h] indices de la paleta, por filas
"""
import argparse
import json
import random
import re
import struct
import sys
from pathlib import Path

TILE, GRID = 16, 15
SRC = TILE * GRID            # 240
CANVAS, CROP = 466, 7        # el mapa expandido mide 480, dibujado en (-7,-7)
FULL = 480
ZOOM = 2.4                   # v3.5: = YBG_ZOOM10 / 10 de yardbg.h (antes 2)
VIEW = int(FULL / ZOOM)      # 200 px de la fuente a la vista
OFF = (SRC - VIEW) // 2      # 20 px de la fuente fuera por cada lado (= YBG_OFF)


def src_to_canvas(v):
    """px de la fuente -> px del lienzo, como yardCellCenter/yardCellEdge de yardbg.h (trunca hacia 0)."""
    return int((v - OFF) * 24 / 10) - CROP


def cell_box(x, y):
    """Rectangulo (x0, y0, x1, y1) de la celda en el lienzo."""
    return (src_to_canvas(x * TILE), src_to_canvas(y * TILE),
            src_to_canvas((x + 1) * TILE) - 1, src_to_canvas((y + 1) * TILE) - 1)
WALK_BYTES = (GRID * GRID + 7) // 8  # 29
FRAME_INSET = 12             # nada se pisa a menos de 12 px del borde redondo
# zonas del HUD en coordenadas del lienzo (x0, y0, x1, y1): hora + monedas, y el dock
CLOCK_ZONE = (140, 20, 326, 110)
DOCK_ZONE = (130, 376, 336, 440)   # = yard.h; el dock va cada 72 px (yard_ui.h)
# colores del dock (yard_ui.h, v3.3: Y_DOCK_PLAY / BAG / SHOP)
DOCK_COLS = ((0xF0, 0xA8, 0x30), (0x5E, 0x8F, 0xE0), (0xD8, 0x60, 0x9A))

HERE = Path(__file__).resolve().parent
DEFAULT_YARD = HERE / 'yard'


class MapError(Exception):
    pass


def _coord(tok, where):
    try:
        c, r = (int(x) for x in tok.split(','))
    except ValueError:
        raise MapError(f'{where}: bad tile coordinate {tok!r} (expected col,row)')
    return c, r


def parse_legend(path):
    """Devuelve (opciones, entradas). Opciones: pack, sheet, autotiles, animals."""
    opts, entries = {}, {}
    for n, raw in enumerate(Path(path).read_text().splitlines(), 1):
        line = raw.split('#', 1)[0].strip()
        if not line:
            continue
        key, eq, val = line.partition('=')
        key, val = key.strip(), val.strip()
        where = f'{path}:{n}'
        if not eq:
            raise MapError(f'{where}: expected "key = value"')
        if key in ('pack', 'sheet', 'autotiles', 'animals'):
            opts[key] = val
            continue
        if len(key) != 1:
            raise MapError(f'{where}: legend key must be one character, got {key!r}')
        if key == '.':
            raise MapError(f'{where}: "." is reserved for "nothing" in the objects layer')
        if key in entries:
            raise MapError(f'{where}: legend key {key!r} is defined twice')
        block = val.endswith('!')
        val = val.rstrip('!').strip()
        weights = None
        if re.search(r'\bweights\b', val):
            val, _, w = val.partition('weights')
            val = val.strip()
            weights = [int(x) for x in w.split()]
        m = re.match(r'autotile\s+"([^"]+)"\s+over\s+(\S)(?:\s+fill\s+(.*))?$', val)
        if m:
            fill = [_coord(t, where) for t in (m.group(3) or '').split()]
            entries[key] = dict(kind='auto', set=m.group(1), over=m.group(2), block=block, fill=fill)
            continue
        toks = val.split()
        if not toks:
            raise MapError(f'{where}: entry {key!r} has no tiles')
        if toks[0] == 'void':
            if len(toks) != 1:
                raise MapError(f'{where}: expected "void" or "void !"')
            entries[key] = dict(kind='void', block=True)
        elif toks[0] == 'draw':
            if len(toks) not in (2, 3):
                raise MapError(f'{where}: expected "draw <name> [WxH]"')
            if toks[1] not in PAINTERS:
                raise MapError(f'{where}: unknown painter {toks[1]!r} (known: {", ".join(sorted(PAINTERS))})')
            w, h = PAINTERS[toks[1]][1]
            if len(toks) == 3:
                if not re.fullmatch(r'\d+x\d+', toks[2]):
                    raise MapError(f'{where}: expected "draw <name> [WxH]"')
                w, h = (int(x) for x in toks[2].split('x'))
            entries[key] = dict(kind='draw', name=toks[1], w=w, h=h, block=block)
        elif toks[0] == 'stamp':
            if len(toks) != 3 or not re.fullmatch(r'\d+x\d+', toks[1]):
                raise MapError(f'{where}: expected "stamp WxH col,row"')
            w, h = (int(x) for x in toks[1].split('x'))
            c, r = _coord(toks[2], where)
            entries[key] = dict(kind='stamp', w=w, h=h, c=c, r=r, block=block)
        else:
            tiles = [_coord(t, where) for t in toks]
            if weights and len(weights) != len(tiles):
                raise MapError(f'{where}: {len(tiles)} tiles but {len(weights)} weights')
            entries[key] = dict(kind='tile', tiles=tiles, weights=weights, block=block)
    for k in ('pack', 'sheet'):
        if k not in opts:
            raise MapError(f'{path}: missing "{k} = ..." line')
    for key, e in entries.items():
        if e['kind'] == 'auto' and entries.get(e['over'], {}).get('kind') not in ('tile', 'void'):
            raise MapError(f'{path}: autotile {key!r} is drawn over {e["over"]!r}, which is not a plain tile or void entry')
    return opts, entries


def parse_map(path):
    """Devuelve (meta, capas). Cada capa: lista de 15 (numero de linea, fila de 15 caracteres)."""
    meta, layers, cur = {}, {}, None
    for n, raw in enumerate(Path(path).read_text().splitlines(), 1):
        line = raw.rstrip()
        if line in ('ground:', 'objects:', 'decal:'):
            cur = line[:-1]
            if cur in layers:
                raise MapError(f'{path}:{n}: layer {cur!r} given twice')
            layers[cur] = []
            continue
        if cur is None:
            body = line.split('#', 1)[0].strip()
            if body:
                k, sep, v = body.partition(':')
                if not sep:
                    raise MapError(f'{path}:{n}: expected "key: value" before the layers')
                k, v = k.strip(), v.strip()
                meta[k] = v
                if k == 'hue':
                    meta.setdefault('hues', []).append(v)   # varias lineas: se aplican en orden
            continue
        if line.strip() == '':
            continue
        if len(line) != GRID:
            raise MapError(f'{path}:{n}: {cur} row has {len(line)} characters, expected {GRID}')
        if len(layers[cur]) == GRID:
            raise MapError(f'{path}:{n}: {cur} layer has more than {GRID} rows')
        layers[cur].append((n, line))
    for k in ('name', 'legend'):
        if k not in meta:
            raise MapError(f'{path}: missing "{k}: ..." line')
    for name in ('ground', 'objects'):
        if len(layers.get(name, [])) != GRID:
            raise MapError(f'{path}: {name} layer has {len(layers.get(name, []))} rows, expected {GRID}')
    if 'decal' in layers and len(layers['decal']) != GRID:
        raise MapError(f'{path}: decal layer has {len(layers["decal"])} rows, expected {GRID}')
    return meta, layers


def load_autotile_sets(path):
    """Tileset .yy de GameMaker: juegos de 16 baldosas. La posicion en la lista es la mascara
    de las esquinas de FONDO (arriba-izq 1, arriba-der 2, abajo-izq 4, abajo-der 8): la 0 es
    terreno macizo y la 15 vacia."""
    d = json.loads(re.sub(r',(\s*[}\]])', r'\1', Path(path).read_text()))
    cols = d['out_columns']
    return {a['name']: [(i % cols, i // cols) if i else None for i in a['tiles']]
            for a in d['autoTileSets']}


def pack_dir(tilesets, opts):
    root = Path(tilesets) / opts['pack']
    sheet = root / opts['sheet']
    if not sheet.exists():
        raise MapError(
            f'tile pack not found: {sheet}\n'
            f'  Download "{opts["pack"]}" (Sunnyside World by Daniel Diggle, itch.io) and unzip it so that\n'
            f'  {root}/ holds its folders (Sunnyside_World_Assets/, Sunnyside_World_Gamemaker/).\n'
            f'  The pack is not in git: its licence forbids redistributing it.')
    return root


def compose(map_path, yard_dir, tilesets):
    """Compone el mapa: (meta, imagen RGB de 240x240, bloqueadas[15][15])."""
    from PIL import Image
    meta, layers = parse_map(map_path)
    legend_path = Path(yard_dir) / 'legends' / f"{meta['legend']}.txt"
    if not legend_path.exists():
        raise MapError(f'{map_path}: legend {meta["legend"]!r} not found ({legend_path})')
    opts, legend = parse_legend(legend_path)
    root = pack_dir(tilesets, opts)
    sheet = Image.open(root / opts['sheet']).convert('RGBA')
    at_sets = load_autotile_sets(root / opts['autotiles']) if 'autotiles' in opts else {}
    try:
        seed = int(meta.get('seed', 0))
    except ValueError:
        raise MapError(f'{map_path}: seed must be a number')
    rng = random.Random(seed)
    img = Image.new('RGBA', (SRC, SRC), (0, 0, 0, 255))
    if 'sky' in meta:
        toks = meta['sky'].split()
        if len(toks) != 2:
            raise MapError(f'{map_path}: expected "sky: RRGGBB RRGGBB" (top, bottom)')
        top, bot = _hex_rgb(toks[0], map_path), _hex_rgb(toks[1], map_path)
        from PIL import ImageDraw
        sd = ImageDraw.Draw(img)
        for yy in range(SRC):
            t = yy / (SRC - 1)
            sd.line((0, yy, SRC, yy), fill=tuple(round(a + (b - a) * t) for a, b in zip(top, bot)) + (255,))
    blocked = [[False] * GRID for _ in range(GRID)]
    covered = [[None] * GRID for _ in range(GRID)]

    def tile(c, r):
        return sheet.crop((c * TILE, r * TILE, (c + 1) * TILE, (r + 1) * TILE))

    ground = [row for _, row in layers['ground']]
    autos = {ch for row in ground for ch in row if legend.get(ch, {}).get('kind') == 'auto'}

    def autotile_pass():
        # un vertice es terreno si alguna de las celdas que lo tocan esta marcada
        for ch in sorted(autos):
            e = legend[ch]
            tiles = at_sets.get(e['set'])
            if tiles is None:
                raise MapError(f'{map_path}: autotile set {e["set"]!r} not found in the .yy file')

            def marked(cx, cy):
                return 0 <= cx < GRID and 0 <= cy < GRID and ground[cy][cx] == ch

            def vert(vx, vy):
                return any(marked(vx - a, vy - b) for a in (0, 1) for b in (0, 1))
            for y in range(GRID):
                for x in range(GRID):
                    bg = ((not vert(x, y)) * 1 | (not vert(x + 1, y)) * 2 |
                          (not vert(x, y + 1)) * 4 | (not vert(x + 1, y + 1)) * 8)
                    if bg == 15 or tiles[bg] is None:
                        continue
                    c, r = tiles[bg]
                    if bg == 0 and e['fill']:
                        c, r = rng.choice(e['fill'])
                    img.alpha_composite(tile(c, r), (x * TILE, y * TILE))
                    if e['block'] and bg == 0:
                        blocked[y][x] = True

    for layer in ('ground', 'objects'):
        if layer == 'objects':
            autotile_pass()  # los bordes, entre el suelo y los objetos
        for y, (lineno, row) in enumerate(layers[layer]):
            for x, ch in enumerate(row):
                if ch == '.' and layer == 'objects':
                    continue
                e = legend.get(ch)
                if e is None:
                    raise MapError(f'{map_path}:{lineno}:{x + 1}: unknown character {ch!r} in {layer}')
                if e['kind'] == 'auto':
                    if layer != 'ground':
                        raise MapError(f'{map_path}:{lineno}:{x + 1}: autotile {ch!r} only allowed in the ground layer')
                    e = legend[e['over']]   # primero el suelo de debajo; los bordes van luego
                if e['kind'] == 'void':
                    if layer != 'ground':
                        raise MapError(f'{map_path}:{lineno}:{x + 1}: void {ch!r} only allowed in the ground layer')
                    if 'sky' not in meta:
                        raise MapError(f'{map_path}:{lineno}:{x + 1}: void {ch!r} needs a "sky:" header line')
                    blocked[y][x] = True
                    continue
                if e['kind'] == 'draw':
                    raise MapError(f'{map_path}:{lineno}:{x + 1}: draw {ch!r} only allowed in the decal layer')
                if e['kind'] == 'tile':
                    c, r = rng.choices(e['tiles'], weights=e['weights'])[0]
                    img.alpha_composite(tile(c, r), (x * TILE, y * TILE))
                    if e['block']:
                        blocked[y][x] = True
                    continue
                if layer == 'ground':
                    raise MapError(f'{map_path}:{lineno}:{x + 1}: stamp {ch!r} not allowed in the ground layer')
                if x + e['w'] > GRID or y + e['h'] > GRID:
                    raise MapError(f'{map_path}:{lineno}:{x + 1}: stamp {ch!r} ({e["w"]}x{e["h"]}) runs off the grid')
                for dy in range(e['h']):
                    for dx in range(e['w']):
                        if covered[y + dy][x + dx]:
                            raise MapError(f'{map_path}:{lineno}:{x + 1}: stamp {ch!r} overlaps {covered[y + dy][x + dx]!r}')
                        covered[y + dy][x + dx] = ch
                        if e['block']:
                            blocked[y + dy][x + dx] = True
                part = sheet.crop((e['c'] * TILE, e['r'] * TILE,
                                   (e['c'] + e['w']) * TILE, (e['r'] + e['h']) * TILE))
                img.alpha_composite(part, (x * TILE, y * TILE))
    # decals: pixel art procedural, encima de los objetos (su propio RNG: no altera el resto del mapa)
    for y, (lineno, row) in enumerate(layers.get('decal', [])):
        for x, ch in enumerate(row):
            if ch == '.':
                continue
            e = legend.get(ch)
            if e is None:
                raise MapError(f'{map_path}:{lineno}:{x + 1}: unknown character {ch!r} in decal')
            if e['kind'] != 'draw':
                raise MapError(f'{map_path}:{lineno}:{x + 1}: {ch!r} is not a "draw" entry (decal layer only takes draw entries)')
            if x + e['w'] > GRID or y + e['h'] > GRID:
                raise MapError(f'{map_path}:{lineno}:{x + 1}: decal {ch!r} ({e["w"]}x{e["h"]}) runs off the grid')
            for dy in range(e['h']):
                for dx in range(e['w']):
                    if covered[y + dy][x + dx]:
                        raise MapError(f'{map_path}:{lineno}:{x + 1}: decal {ch!r} overlaps {covered[y + dy][x + dx]!r}')
                    covered[y + dy][x + dx] = ch
                    if e['block']:
                        blocked[y + dy][x + dx] = True
            drng = random.Random(seed * 1009 + y * GRID + x)
            img.alpha_composite(PAINTERS[e['name']][0](e['w'], e['h'], drng), (x * TILE, y * TILE))
    return meta, atmosphere(img.convert('RGB'), meta, map_path, seed), blocked


def _hex_rgb(tok, where):
    m = re.fullmatch(r'#?([0-9a-fA-F]{6})', tok)
    if not m:
        raise MapError(f'{where}: bad colour {tok!r} (expected RRGGBB)')
    v = int(m.group(1), 16)
    return (v >> 16) & 255, (v >> 8) & 255, v & 255


# ---- decals: pixel art procedural (v3.15). Paleta corta y contorno oscuro, al estilo de las baldosas de 16 px ----
def _canvas(w, h):
    from PIL import Image, ImageDraw
    im = Image.new('RGBA', (w * TILE, h * TILE), (0, 0, 0, 0))
    return im, ImageDraw.Draw(im)


def paint_lava_crack(w, h, rng):
    """Grieta de lava: linea quebrada oscura con el centro naranja y unas chispas amarillas."""
    im, d = _canvas(w, h)
    W, H = w * TILE, h * TILE
    path = []
    x, y = 1, rng.randint(H // 3, 2 * H // 3)
    while x < W - 1:
        path.append((x, y))
        x += 1
        if rng.random() < 0.5:
            y = max(3, min(H - 4, y + rng.choice((-1, 1))))
        if rng.random() < 0.12:
            y = max(3, min(H - 4, y + rng.choice((-2, 2))))
    branch = rng.randrange(len(path) // 3, 2 * len(path) // 3)
    bx, by = path[branch]
    sgn = rng.choice((-1, 1))
    for i in range(1, rng.randint(4, 6)):
        path.append((bx + rng.choice((0, 1)), by + sgn * i))
    for px_, py_ in path:                 # borde oscuro (2 px), luego el naranja y el brillo
        d.rectangle((px_ - 1, py_ - 1, px_ + 1, py_ + 1), fill=(58, 16, 12, 255))
    for px_, py_ in path:
        d.point((px_, py_), fill=(255, 106, 19, 255))
    for i, (px_, py_) in enumerate(path):
        if i % 5 == 2:
            d.point((px_, py_), fill=(255, 214, 74, 255))
    return im


def paint_bolt(w, h, rng):
    """Marca de rayo en el suelo: chamusquina oscura con un rayo amarillo."""
    im, d = _canvas(w, h)
    d.ellipse((1, 3, 14, 14), fill=(44, 46, 58, 255))
    d.ellipse((3, 5, 12, 12), fill=(30, 32, 42, 255))
    bolt = [(9, 0), (3, 9), (7, 9), (5, 15), (13, 5), (8, 5), (12, 0)]
    d.polygon(bolt, fill=(255, 224, 58, 255), outline=(74, 56, 8, 255))
    d.line((9, 2, 6, 7), fill=(255, 250, 190, 255))
    return im


def paint_pylon(w, h, rng):
    """Torre electrica de celosia (acero gris con aisladores): base ancha, travesanos y cables."""
    im, d = _canvas(w, h)
    W, H = w * TILE, h * TILE
    cx = W // 2
    dark, mid, lit = (30, 34, 44, 255), (92, 100, 116, 255), (160, 170, 184, 255)
    top, base = 5, H - 2
    hw_top, hw_base = 2, W // 2 - 3

    def half(y):
        return hw_top + (hw_base - hw_top) * (y - top) // (base - top)
    for y in range(top, base + 1):         # patas (contorno oscuro y centro claro)
        for sx in (-1, 1):
            xx = cx + sx * half(y)
            d.rectangle((xx - 1, y, xx + 1, y), fill=dark)
    for y in range(top, base + 1):
        for sx in (-1, 1):
            d.point((cx + sx * half(y), y), fill=mid)
    levels = [top + (base - top) * i // 4 for i in range(5)]
    for a, b in zip(levels, levels[1:]):   # diagonales en X entre travesanos
        for pts in (((cx - half(a), a), (cx + half(b), b)), ((cx + half(a), a), (cx - half(b), b))):
            d.line(pts, fill=dark, width=2)
            d.line(pts, fill=lit, width=1)
    for y in levels[1:]:
        d.rectangle((cx - half(y) - 1, y - 1, cx + half(y) + 1, y), fill=dark)
        d.line((cx - half(y), y - 1, cx + half(y), y - 1), fill=mid)
    # brazos con aisladores y un trozo de cable
    for y, ext in ((levels[1], 10), (levels[2], 13)):
        d.rectangle((cx - ext, y - 3, cx + ext, y - 2), fill=dark)
        d.line((cx - ext + 1, y - 3, cx + ext - 1, y - 3), fill=lit)
        for sx in (-1, 1):
            d.rectangle((cx + sx * ext - 1, y - 1, cx + sx * ext, y + 2), fill=(236, 226, 190, 255), outline=dark)
    d.rectangle((cx - 2, 0, cx + 2, top), fill=dark)       # la punta
    d.rectangle((cx - 1, 1, cx + 1, top - 1), fill=lit)
    d.rectangle((cx - 4, base - 1, cx + 4, base), fill=dark)   # zapata
    return im


def paint_smokestack(w, h, rng):
    """Chimenea de ladrillo con bandas y una bocanada de humo morado arriba."""
    im, d = _canvas(w, h)
    W, H = w * TILE, h * TILE
    cx = W // 2
    dark = (36, 22, 30, 255)
    y0 = 14
    for y in range(y0, H - 3):             # fuste algo mas ancho abajo
        hw = 5 + (y - y0) * 3 // (H - 3 - y0)
        band = ((y - y0) // 6) % 2 == 0
        d.rectangle((cx - hw, y, cx + hw, y), fill=(196, 62, 52, 255) if band else (222, 212, 200, 255))
        d.point((cx - hw, y), fill=dark)
        d.point((cx + hw, y), fill=dark)
        d.point((cx + hw - 2, y), fill=(140, 40, 40, 255) if band else (170, 160, 156, 255))
    d.rectangle((cx - 7, y0 - 2, cx + 7, y0 + 1), fill=(70, 66, 80, 255), outline=dark)   # boca
    d.rectangle((cx - 9, H - 5, cx + 9, H - 1), fill=(86, 82, 96, 255), outline=dark)    # base
    d.line((cx - 8, H - 4, cx + 8, H - 4), fill=(130, 126, 142, 255))
    for (ox, oy, r, col) in ((cx - 1, 9, 4, (154, 132, 176, 255)), (cx + 5, 5, 4, (178, 156, 200, 255)),
                             (cx + 1, 3, 3, (206, 188, 224, 255)), (cx + 9, 1, 2, (190, 170, 212, 255))):
        d.ellipse((ox - r, oy - r, ox + r, oy + r), fill=col, outline=(96, 72, 120, 255))
    return im


def paint_cloud_puff(w, h, rng):
    """Nube pequena y blanda: bolas blancas con sombra azulada abajo."""
    im, d = _canvas(w, h)
    W, H = w * TILE, h * TILE
    n = max(3, w * 2)
    blobs = []
    for i in range(n):
        r = rng.randint(4, 6)
        cx = 5 + (W - 10) * i // max(1, n - 1)
        cy = H // 2 + rng.randint(-1, 2) - (3 if 0 < i < n - 1 else 0)
        blobs.append((cx, cy, r))
    for cx, cy, r in blobs:                # sombra, luego el cuerpo y el brillo
        d.ellipse((cx - r, cy - r + 2, cx + r, cy + r + 2), fill=(176, 198, 226, 255))
    d.rectangle((blobs[0][0], H // 2 + 1, blobs[-1][0], H // 2 + 6), fill=(176, 198, 226, 255))
    for cx, cy, r in blobs:
        d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(248, 250, 255, 255))
    d.rectangle((blobs[0][0], H // 2 - 1, blobs[-1][0], H // 2 + 3), fill=(248, 250, 255, 255))
    return im


def paint_vent(w, h, rng):
    """Rejilla de ventilacion toxica: marco oscuro, barras y brillo verde."""
    im, d = _canvas(w, h)
    d.rectangle((1, 2, 14, 14), fill=(28, 36, 30, 255))
    d.rectangle((2, 3, 13, 13), fill=(70, 80, 74, 255))
    d.rectangle((3, 4, 12, 12), fill=(190, 255, 50, 255))
    for y in (5, 8, 11):
        d.rectangle((3, y, 12, y + 1), fill=(28, 36, 30, 255))
    d.point((4, 9), fill=(245, 255, 170, 255))
    d.point((9, 6), fill=(245, 255, 170, 255))
    d.rectangle((6, 0, 7, 1), fill=(200, 240, 80, 255))     # un hilo de vapor
    d.point((8, 0), fill=(200, 240, 80, 255))
    return im


def paint_bubbles(w, h, rng):
    """Burbujas y destellos sobre un liquido (lima claro: sobrevive a las reglas de tono verdes)."""
    im, d = _canvas(w, h)
    W, H = w * TILE, h * TILE
    ring, glint = (205, 255, 110, 255), (255, 255, 215, 255)
    for _ in range(rng.randint(2, 3) * w * h):
        r = rng.choice((1, 2, 2, 3))
        x, y = rng.randint(r + 1, W - r - 2), rng.randint(r + 1, H - r - 2)
        d.ellipse((x - r, y - r, x + r, y + r), outline=ring)
        d.point((x - r + 1, y - r + 1), fill=glint)
    for _ in range(w * h * 2):
        d.point((rng.randrange(1, W - 1), rng.randrange(1, H - 1)), fill=glint)
    return im


def paint_floor_crack(w, h, rng):
    """Grieta en el suelo de piedra: linea quebrada oscura con un reborde claro."""
    im, d = _canvas(w, h)
    W, H = w * TILE, h * TILE
    x, y = rng.randint(1, 4), rng.randint(3, H - 4)
    pts = []
    while x < W - 2:
        pts.append((x, y))
        x += rng.choice((1, 1, 2))
        y = max(2, min(H - 3, y + rng.choice((-2, -1, 0, 1, 2))))
    for px_, py_ in pts:
        d.point((px_, py_ + 1), fill=(150, 162, 188, 255))
    for px_, py_ in pts:
        d.point((px_, py_), fill=(52, 58, 80, 255))
    return im


def paint_compass(w, h, rng):
    """Mosaico de rosa de los vientos: aro oscuro, aro claro, estrella dorada y centro rojo."""
    im, d = _canvas(w, h)
    W, H = w * TILE, h * TILE
    cx, cy = W // 2, H // 2
    r = min(W, H) // 2 - 1
    dark, cream, blue, gold, red = (44, 54, 84, 255), (222, 228, 240, 255), (104, 126, 170, 255), (240, 200, 80, 255), (200, 56, 64, 255)
    d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=dark)
    d.ellipse((cx - r + 2, cy - r + 2, cx + r - 2, cy + r - 2), fill=cream)
    d.ellipse((cx - r + 5, cy - r + 5, cx + r - 5, cy + r - 5), fill=blue)
    k = r - 3
    for sx, sy, ln, col in ((0, -1, k, gold), (0, 1, k, gold), (1, 0, k, gold), (-1, 0, k, gold),
                            (1, -1, k * 2 // 3, cream), (-1, -1, k * 2 // 3, cream),
                            (1, 1, k * 2 // 3, cream), (-1, 1, k * 2 // 3, cream)):
        if sx and sy:
            tip = (cx + sx * ln * 7 // 10, cy + sy * ln * 7 // 10)
            a, b = (cx + sx * 3, cy), (cx, cy + sy * 3)
        else:
            tip = (cx + sx * ln, cy + sy * ln)
            a, b = (cx + sy * 3, cy + sx * 3), (cx - sy * 3, cy - sx * 3)
        d.polygon([tip, a, b], fill=col, outline=dark)
    d.ellipse((cx - 3, cy - 3, cx + 3, cy + 3), fill=red, outline=dark)
    return im


def paint_banner(w, h, rng):
    """Estandarte colgado de un mastil, con franja dorada y cola de golondrina."""
    im, d = _canvas(w, h)
    W, H = w * TILE, h * TILE
    dark, pole = (52, 26, 30, 255), (120, 80, 50, 255)
    px0 = W // 2 - 6
    d.rectangle((px0, 3, px0 + 1, H - 2), fill=pole, outline=dark)
    d.ellipse((px0 - 1, 0, px0 + 2, 3), fill=(240, 200, 80, 255), outline=dark)
    x0, x1, y0, y1 = px0 + 2, px0 + 11, 5, H - 8
    d.polygon([(x0, y0), (x1, y0), (x1, y1), ((x0 + x1) // 2, y1 - 4), (x0, y1)], fill=(200, 60, 72, 255), outline=dark)
    d.line((x0 + 1, y0 + 4, x1 - 1, y0 + 4), fill=(240, 200, 80, 255))
    d.line((x0 + 1, y0 + 5, x1 - 1, y0 + 5), fill=(240, 200, 80, 255))
    d.rectangle((x0 + 3, y0 + 8, x1 - 3, y0 + 10), fill=(250, 236, 200, 255))
    return im


# nombre -> (pintor, tamano por defecto en celdas)
PAINTERS = {
    'lava_crack': (paint_lava_crack, (1, 1)),
    'bolt': (paint_bolt, (1, 1)),
    'pylon': (paint_pylon, (2, 3)),
    'smokestack': (paint_smokestack, (2, 3)),
    'cloud_puff': (paint_cloud_puff, (2, 1)),
    'vent': (paint_vent, (1, 1)),
    'bubbles': (paint_bubbles, (1, 1)),
    'floor_crack': (paint_floor_crack, (1, 1)),
    'compass': (paint_compass, (3, 3)),
    'banner': (paint_banner, (1, 2)),
}


def atmosphere(img, meta, map_path, seed):
    """Ambiente opcional del mapa, horneado en la imagen (el firmware no cambia):
      hue: A-B T [keep]   los tonos entre A y B grados (p. ej. la hierba verde) pasan a rondar T; mas
                          apagados, salvo con "keep" (conserva saturacion y valor: lava) o "dark" (conserva la
                          saturacion y deja el valor al 55 %: lodo toxico, rocas de obsidiana).
                          Puede repetirse: cada linea se aplica en orden.
      tint: RRGGBB N      mezcla toda la imagen un N % hacia ese color (p. ej. morado fantasmal)
      mist: N [RRGGBB]    N jirones de niebla suaves (semilla fija), del color dado o lavanda
    """
    from PIL import Image, ImageDraw, ImageFilter
    for spec in meta.get('hues', []):
        m = re.fullmatch(r'(\d+)-(\d+)\s+(\d+)(?:\s+(keep|dark|murk))?', spec)
        if not m:
            raise MapError(f'{map_path}: expected "hue: from-to target [keep|dark|murk]" (degrees)')
        lo, hi, tgt = (int(v) for v in m.groups()[:3])
        flag = m.group(4)   # None: apagado; keep: igual; dark: igual de vivo pero mas oscuro
        mid = (lo + hi) / 2
        hsv = img.convert('HSV')
        hd, sd, vd = (bytearray(c.tobytes()) for c in hsv.split())
        for i, h in enumerate(hd):
            deg = h * 360 / 256
            if lo <= deg <= hi:   # el tono nuevo, mas apagado y algo mas oscuro (no un violeta chillon)
                hd[i] = int(((tgt + (deg - mid) * 0.5) % 360) * 256 / 360)
                if flag is None:
                    sd[i] = sd[i] * 45 // 100
                    vd[i] = vd[i] * 80 // 100
                elif flag == 'dark':
                    vd[i] = vd[i] * 55 // 100
                elif flag == 'murk':   # liquido turbio: poca saturacion y casi negro
                    sd[i] = sd[i] * 55 // 100
                    vd[i] = vd[i] * 30 // 100
        chans = [Image.frombytes('L', img.size, bytes(c)) for c in (hd, sd, vd)]
        img = Image.merge('HSV', chans).convert('RGB')
    if 'tint' in meta:
        toks = meta['tint'].split()
        if len(toks) != 2 or not toks[1].isdigit() or not 0 <= int(toks[1]) <= 100:
            raise MapError(f'{map_path}: expected "tint: RRGGBB percent"')
        col = _hex_rgb(toks[0], map_path)
        img = Image.blend(img, Image.new('RGB', img.size, col), int(toks[1]) / 100)
    if 'mist' in meta:
        toks = meta['mist'].split()
        if not toks or not toks[0].isdigit() or len(toks) > 2:
            raise MapError(f'{map_path}: expected "mist: count [RRGGBB]"')
        col = _hex_rgb(toks[1], map_path) if len(toks) == 2 else (214, 196, 255)
        rng = random.Random(seed * 7919 + 1)
        alpha = Image.new('L', img.size, 0)
        d = ImageDraw.Draw(alpha)
        for _ in range(int(toks[0])):
            # jirones alargados en horizontal, como niebla baja
            cx, cy = rng.uniform(0, SRC), rng.uniform(20, SRC - 20)
            rx, ry = rng.uniform(30, 60), rng.uniform(8, 16)
            d.ellipse((cx - rx, cy - ry, cx + rx, cy + ry), fill=rng.randint(110, 170))
        alpha = alpha.filter(ImageFilter.GaussianBlur(9))
        img = Image.composite(Image.new('RGB', img.size, col), img, alpha)
    return img


def outside_or_hud(cx, cy):
    """True si el centro de la celda (en px de la fuente) cae fuera del circulo o en una zona del HUD."""
    px, py = src_to_canvas(cx), src_to_canvas(cy)
    if (px - CANVAS / 2) ** 2 + (py - CANVAS / 2) ** 2 > (CANVAS / 2 - FRAME_INSET) ** 2:
        return True
    for x0, y0, x1, y1 in (CLOCK_ZONE, DOCK_ZONE):
        if x0 <= px <= x1 and y0 <= py <= y1:
            return True
    return False


def walk_mask(blocked):
    bits = bytearray(WALK_BYTES)
    for y in range(GRID):
        for x in range(GRID):
            if blocked[y][x] or outside_or_hud(x * TILE + TILE // 2, y * TILE + TILE // 2):
                k = y * GRID + x
                bits[k // 8] |= 1 << (k % 8)
    return bytes(bits)


def rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def encode_tpy1(img, blocked):
    """Devuelve (bytes del TPY1, colores usados)."""
    from PIL import Image
    q = img.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    px = q.tobytes()                      # un byte por pixel: el indice de la paleta
    pal = q.getpalette()[:3 * 256]
    count = max(px) + 1
    pal565 = b''.join(struct.pack('<H', rgb565(*pal[i * 3:i * 3 + 3])) for i in range(count))
    data = b'TPY1' + bytes([1, SRC, SRC, count % 256]) + pal565 + walk_mask(blocked) + px
    return data, len(set(px))


def decode_tpy1(data):
    """Lector de referencia (el del firmware es yardbg.h): (paleta RGB565, mascara, pixeles)."""
    if len(data) < 8 or data[:4] != b'TPY1' or data[4] != 1:
        raise ValueError('not a TPY1 v1 file')
    w, h, n = data[5], data[6], data[7] or 256
    off = 8
    pal = list(struct.unpack_from(f'<{n}H', data, off))
    off += 2 * n
    walk = data[off:off + WALK_BYTES]
    off += WALK_BYTES
    px = data[off:off + w * h]
    if len(px) != w * h or len(data) != off + w * h:
        raise ValueError('TPY1 file has the wrong length')
    return pal, walk, px


def to_canvas(img):
    from PIL import Image
    # vecino mas cercano como yardBgExpandFull: el pixel X sale del OFF + X * 10 // 24 de la fuente
    src = img.convert('RGB')
    big = Image.new('RGB', (FULL, FULL))
    idx = [OFF + X * 10 // 24 for X in range(FULL)]
    sp, bp = src.load(), big.load()
    for Y in range(FULL):
        sy = idx[Y]
        for X in range(FULL):
            bp[X, Y] = sp[idx[X], sy]
    return big.crop((CROP, CROP, CROP + CANVAS, CROP + CANVAS))


def round_mask(canvas):
    from PIL import Image, ImageDraw
    mask = Image.new('L', canvas.size, 0)
    ImageDraw.Draw(mask).ellipse((0, 0, CANVAS - 1, CANVAS - 1), fill=255)
    out = Image.new('RGB', canvas.size, (0, 0, 0))
    out.paste(canvas, (0, 0), mask)
    return out


def _font(size):
    from PIL import ImageFont
    for p in ('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',
              '/usr/share/fonts/TTF/DejaVuSans-Bold.ttf',
              '/Library/Fonts/Arial Bold.ttf'):
        if Path(p).exists():
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()


def add_mock_hud(im, sprites):
    """Hora, monedas y el dock (Cuidar / Jugar / Mochila), aproximados con fuentes del PC."""
    from PIL import ImageDraw
    d = ImageDraw.Draw(im)
    f_big, f_small = _font(46), _font(17)
    t = '12:30'
    w = d.textlength(t, font=f_big)
    d.text(((CANVAS - w) / 2 + 2, 30 + 2), t, font=f_big, fill=(16, 18, 26))
    d.text(((CANVAS - w) / 2, 30), t, font=f_big, fill=(255, 255, 255))
    d.ellipse((205, 76, 223, 94), fill=(242, 194, 48), outline=(184, 134, 11), width=2)
    d.text((229, 74), '117', font=f_small, fill=(255, 255, 255), stroke_width=2, stroke_fill=(16, 18, 26))
    for i, col in enumerate(DOCK_COLS):
        cx = CANVAS // 2 + (i - 1) * 72
        d.ellipse((cx - 20, 404 - 19, cx + 24, 404 + 25), fill=(16, 18, 26))
        d.ellipse((cx - 22, 404 - 22, cx + 22, 404 + 22), fill=col, outline=(255, 255, 255), width=1)
    for spr, xy in sprites:
        im.paste(spr, xy, spr)


def debug_image(canvas, blocked, spots=None):
    """La imagen de depuracion: celdas bloqueadas en rojo, marco/HUD en azul y (v3.20) los muebles de casa."""
    from PIL import Image, ImageDraw
    dbg = canvas.copy().convert('RGBA')
    ov = Image.new('RGBA', dbg.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(ov)
    for y in range(GRID):
        for x in range(GRID):
            if blocked[y][x]:
                d.rectangle(cell_box(x, y), fill=(255, 0, 0, 70))
            elif outside_or_hud(x * TILE + TILE // 2, y * TILE + TILE // 2):
                d.rectangle(cell_box(x, y), fill=(0, 0, 255, 40))
    for z in (CLOCK_ZONE, DOCK_ZONE):
        d.rectangle(z, outline=(255, 255, 0, 255), width=2)
    for i in range(GRID + 1):
        e = src_to_canvas(i * TILE)
        d.line((e, 0, e, CANVAS), fill=(0, 0, 0, 50))
        d.line((0, e, CANVAS, e), fill=(0, 0, 0, 50))
    if spots:
        draw_spots(d, spots)
    dbg.alpha_composite(ov)
    return round_mask(dbg.convert('RGB'))


# v3.20: los muebles de casa (yard_spots.h): la pareja de camas, el gimnasio y el juguete de cada mapa, en celdas
SPOTS_H = HERE.parent / 'yard_spots.h'
SPOT_COLS = {'B0': (40, 90, 255), 'B1': (40, 90, 255), 'G': (255, 140, 0), 'T': (0, 200, 90)}


def read_spots(path=SPOTS_H):
    """La tabla YARD_SPOTS de yard_spots.h: lista por mapa (0 = suelo liso) de {'B0': (x, y), 'B1', 'G', 'T'}."""
    text = Path(path).read_text()
    body = text[text.index('YARD_SPOTS['):]
    body = body[body.index('{') + 1:body.index('};')]
    out = []
    num = r'\s*(\d+)\s*'
    for m in re.finditer(r'\{' + ','.join([num] * 6) + r'\}', body):
        bx, by, gx, gy, tx, ty = (int(v) for v in m.groups())
        out.append({'B0': (bx, by), 'B1': (bx + 1, by), 'G': (gx, gy), 'T': (tx, ty)})
    return out


def draw_spots(d, spots):
    """Los sitios de los muebles sobre la imagen de depuracion: un recuadro de color con su nombre."""
    for name, (x, y) in spots.items():
        box = cell_box(x, y)
        col = SPOT_COLS[name]
        d.rectangle(box, outline=col + (255,), width=3, fill=col + (70,))
        d.text((box[0] + 4, box[1] + 3), name, fill=(255, 255, 255, 255))


def spots_images(out, spots_h=SPOTS_H):
    """--spots: los sitios de los muebles sobre los .bin ya hechos de out/ (NN-name_spots.png), sin el paquete de
    baldosas. La mascara es la del .bin (con el marco ya puesto; las celdas sueltas no se quitan)."""
    from PIL import Image
    table = read_spots(spots_h)
    done = []
    for b in sorted(Path(out).glob('[0-9][0-9]-*.bin')):
        n = int(b.name[:2])
        if n >= len(table):
            raise MapError(f'{b.name}: no row {n} in {spots_h}')
        pal, walk, px = decode_tpy1(b.read_bytes())
        rgb = [(((c >> 11) & 31) << 3, ((c >> 5) & 63) << 2, (c & 31) << 3) for c in pal]
        img = Image.new('RGB', (SRC, SRC))
        img.putdata([rgb[i] for i in px])
        blocked = [[(walk[(y * GRID + x) // 8] >> ((y * GRID + x) % 8)) & 1 for x in range(GRID)] for y in range(GRID)]
        path = Path(out) / f'{b.stem}_spots.png'
        debug_image(to_canvas(img), blocked, table[n]).save(path)
        done.append(path)
    return done


def scale_sprites(yard_dir, tilesets, legend_name):
    """Dos animales del paquete para ver la escala en la vista previa (si estan)."""
    from PIL import Image
    opts, _ = parse_legend(Path(yard_dir) / 'legends' / f'{legend_name}.txt')
    if 'animals' not in opts:
        return []
    d = Path(tilesets) / opts['pack'] / opts['animals']
    out = []
    for name, xy in (('spr_deco_sheep_01_strip4.png', (200, 230)), ('spr_deco_chicken_01_strip4.png', (270, 270))):
        if (d / name).exists():
            spr = Image.open(d / name).convert('RGBA').crop((0, 0, 32, 32))
            out.append((spr.resize((77, 77), Image.NEAREST), xy))
    return out


def map_files(yard_dir, only):
    maps = sorted((Path(yard_dir) / 'maps').glob('*.txt'))
    for mp in maps:
        if not re.fullmatch(r'\d\d-[a-z0-9_]+', mp.stem):
            raise MapError(f'{mp}: map files are named NN-name.txt (two digits, lower case)')
    if only:
        want = {o.zfill(2) for o in only}
        maps = [m for m in maps if m.stem[:2] in want]
        missing = want - {m.stem[:2] for m in maps}
        if missing:
            raise MapError(f'no map numbered {", ".join(sorted(missing))} in {yard_dir}/maps')
    return maps


def main(argv=None):
    ap = argparse.ArgumentParser(description='Pack yard maps into TPY1 files for the SD card.')
    ap.add_argument('maps', nargs='*', help='map numbers to pack (default: all)')
    ap.add_argument('--yard', default=str(DEFAULT_YARD), help='folder with legends/ and maps/ (default: tools/yard)')
    ap.add_argument('--tilesets', help='folder with the unzipped packs (default: <yard>/tilesets)')
    ap.add_argument('--out', help='output folder (default: <yard>/out)')
    ap.add_argument('--no-preview', action='store_true', help='only write the .bin files')
    ap.add_argument('--spots', action='store_true',
                    help='only draw the home furniture spots (yard_spots.h) over the .bin files already in --out')
    args = ap.parse_args(argv)
    yard = Path(args.yard)
    tilesets = Path(args.tilesets) if args.tilesets else yard / 'tilesets'
    out = Path(args.out) if args.out else yard / 'out'
    out.mkdir(parents=True, exist_ok=True)
    if args.spots:
        for path in spots_images(out):
            print(path)
        return
    table = read_spots() if SPOTS_H.exists() else []
    for mp in map_files(yard, args.maps):
        meta, img, blocked = compose(mp, yard, tilesets)
        data, colours = encode_tpy1(img, blocked)
        (out / f'{mp.stem}.bin').write_bytes(data)
        if not args.no_preview:
            canvas = to_canvas(img)
            prev = canvas.copy()
            add_mock_hud(prev, scale_sprites(yard, tilesets, meta['legend']))
            round_mask(prev).save(out / f'{mp.stem}_preview.png')
            n = int(mp.stem[:2])
            debug_image(canvas, blocked, table[n] if n < len(table) else None).save(out / f'{mp.stem}_debug.png')
        print(f'{mp.stem}: {meta["name"]}  {colours} colours  {len(data)} bytes')


if __name__ == '__main__':
    try:
        main()
    except MapError as e:
        sys.exit(f'error: {e}')
