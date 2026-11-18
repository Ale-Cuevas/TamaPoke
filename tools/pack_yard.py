#!/usr/bin/env python3
"""Fondos del patio (v2.0): mapas de texto + paquete de baldosas -> ficheros TPY1 para la SD.

Lee tools/yard/maps/NN-nombre.txt y su leyenda (tools/yard/legends/), compone una imagen de
240x240 con baldosas de 16 px y escribe en tools/yard/out/:
  NN-nombre.bin           fichero TPY1 (paleta + mascara de paso + pixeles indexados)
  NN-nombre_preview.png   466x466 redondo (x2,4 del centro, recorte de 7, mascara, HUD de muestra)
  NN-nombre_debug.png     lo mismo con la rejilla, las celdas bloqueadas y las zonas del HUD

Ambiente opcional en la cabecera del mapa: "hue: A-B T", "tint: RRGGBB N" (mezcla N %) y "mist: N [RRGGBB]".

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
        if toks[0] == 'stamp':
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
        if e['kind'] == 'auto' and entries.get(e['over'], {}).get('kind') != 'tile':
            raise MapError(f'{path}: autotile {key!r} is drawn over {e["over"]!r}, which is not a plain tile entry')
    return opts, entries


def parse_map(path):
    """Devuelve (meta, capas). Cada capa: lista de 15 (numero de linea, fila de 15 caracteres)."""
    meta, layers, cur = {}, {}, None
    for n, raw in enumerate(Path(path).read_text().splitlines(), 1):
        line = raw.rstrip()
        if line in ('ground:', 'objects:'):
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
                meta[k.strip()] = v.strip()
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
    return meta, atmosphere(img.convert('RGB'), meta, map_path, seed), blocked


def _hex_rgb(tok, where):
    m = re.fullmatch(r'#?([0-9a-fA-F]{6})', tok)
    if not m:
        raise MapError(f'{where}: bad colour {tok!r} (expected RRGGBB)')
    v = int(m.group(1), 16)
    return (v >> 16) & 255, (v >> 8) & 255, v & 255


def atmosphere(img, meta, map_path, seed):
    """Ambiente opcional del mapa, horneado en la imagen (el firmware no cambia):
      hue: A-B T          los tonos entre A y B grados (p. ej. la hierba verde) pasan a rondar T
      tint: RRGGBB N      mezcla toda la imagen un N % hacia ese color (p. ej. morado fantasmal)
      mist: N [RRGGBB]    N jirones de niebla suaves (semilla fija), del color dado o lavanda
    """
    from PIL import Image, ImageDraw, ImageFilter
    if 'hue' in meta:
        m = re.fullmatch(r'(\d+)-(\d+)\s+(\d+)', meta['hue'])
        if not m:
            raise MapError(f'{map_path}: expected "hue: from-to target" (degrees)')
        lo, hi, tgt = (int(v) for v in m.groups())
        mid = (lo + hi) / 2
        hsv = img.convert('HSV')
        hd, sd, vd = (bytearray(c.tobytes()) for c in hsv.split())
        for i, h in enumerate(hd):
            deg = h * 360 / 256
            if lo <= deg <= hi:   # el tono nuevo, mas apagado y algo mas oscuro (no un violeta chillon)
                hd[i] = int(((tgt + (deg - mid) * 0.5) % 360) * 256 / 360)
                sd[i] = sd[i] * 45 // 100
                vd[i] = vd[i] * 80 // 100
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


def debug_image(canvas, blocked):
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
    dbg.alpha_composite(ov)
    return round_mask(dbg.convert('RGB'))


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
    args = ap.parse_args(argv)
    yard = Path(args.yard)
    tilesets = Path(args.tilesets) if args.tilesets else yard / 'tilesets'
    out = Path(args.out) if args.out else yard / 'out'
    out.mkdir(parents=True, exist_ok=True)
    for mp in map_files(yard, args.maps):
        meta, img, blocked = compose(mp, yard, tilesets)
        data, colours = encode_tpy1(img, blocked)
        (out / f'{mp.stem}.bin').write_bytes(data)
        if not args.no_preview:
            canvas = to_canvas(img)
            prev = canvas.copy()
            add_mock_hud(prev, scale_sprites(yard, tilesets, meta['legend']))
            round_mask(prev).save(out / f'{mp.stem}_preview.png')
            debug_image(canvas, blocked).save(out / f'{mp.stem}_debug.png')
        print(f'{mp.stem}: {meta["name"]}  {colours} colours  {len(data)} bytes')


if __name__ == '__main__':
    try:
        main()
    except MapError as e:
        sys.exit(f'error: {e}')
