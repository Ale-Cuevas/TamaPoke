#!/usr/bin/env python3
"""Tests de las herramientas del taller (tools/) y de los ficheros generados.

  python3 test/test_tools.py
"""
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS = os.path.join(ROOT, 'tools')
sys.path.insert(0, TOOLS)


class TestToolsCompile(unittest.TestCase):
    def test_todos_los_scripts_compilan(self):
        scripts = sorted(f for f in os.listdir(TOOLS) if f.endswith('.py'))
        self.assertTrue(scripts, 'no se encontro ningun script en tools/')
        for name in scripts:
            with self.subTest(script=name):
                with open(os.path.join(TOOLS, name), encoding='utf-8') as fh:
                    src = fh.read()
                try:
                    compile(src, name, 'exec')
                except SyntaxError as e:
                    self.fail(f'{name} no compila: {e}')

    def test_los_scripts_ejecutables_declaran_shebang(self):
        for name in sorted(f for f in os.listdir(TOOLS) if f.endswith('.py')):
            path = os.path.join(TOOLS, name)
            if not os.access(path, os.X_OK):
                continue
            with open(path) as fh:
                self.assertTrue(fh.readline().startswith('#!'),
                                f'{name} es ejecutable pero no tiene shebang')


class TestDexData(unittest.TestCase):
    """Invariantes de la fuente de datos de la Pokedex."""

    @classmethod
    def setUpClass(cls):
        from dex_data import DEX, TYPE_ACCENTS, TYPE_ORDER, TYPE2, CLASSIC, RARE, LEGENDARY, SLUGS
        from dex_stats import BASE_STATS
        from dex_names import LOCAL_NAMES
        cls.DEX, cls.ACCENTS, cls.CLASSIC = DEX, TYPE_ACCENTS, CLASSIC
        cls.ORDER, cls.TYPE2 = TYPE_ORDER, TYPE2
        cls.RARE, cls.LEGENDARY, cls.SLUGS = RARE, LEGENDARY, SLUGS
        cls.STATS, cls.NAMES = BASE_STATS, LOCAL_NAMES
        cls.byNum = {row[0]: row for row in DEX}

    def test_estan_las_151_especies_sin_huecos_ni_repetidos(self):
        nums = [row[0] for row in self.DEX]
        self.assertEqual(len(nums), 151)
        self.assertEqual(sorted(nums), list(range(1, 152)))

    def test_los_slugs_son_unicos_y_en_minusculas(self):
        slugs = [row[1] for row in self.DEX]
        self.assertEqual(len(set(slugs)), len(slugs), 'hay slugs repetidos')
        for s in slugs:
            self.assertEqual(s, s.lower(), f'slug con mayusculas: {s}')
            self.assertTrue(s.isascii(), f'slug no ASCII: {s}')

    def test_los_nombres_de_pantalla_son_ascii_en_mayusculas(self):
        # la fuente GFX del firmware no tiene acentos ni minusculas
        for num, slug, display, *_ in self.DEX:
            self.assertTrue(display.isascii(), f'{num} {display} no es ASCII')
            self.assertEqual(display, display.upper(), f'{num} {display} lleva minusculas')
            self.assertLessEqual(len(display), 12, f'{num} {display} no cabe en pantalla')

    def test_las_evoluciones_apuntan_a_especies_reales(self):
        for num, slug, display, tipo, evo, lvl in self.DEX:
            if evo:
                self.assertIn(evo, self.byNum, f'{display} evoluciona a {evo}, que no existe')
                self.assertNotEqual(evo, num, f'{display} evoluciona a si mismo')
                self.assertGreater(lvl, 0, f'{display} evoluciona sin nivel')
            else:
                self.assertEqual(lvl, 0, f'{display} es forma final pero tiene nivel {lvl}')

    def test_ninguna_cadena_evolutiva_hace_bucle(self):
        for num in self.byNum:
            seen, cur = set(), num
            while cur and self.byNum[cur][4]:
                self.assertNotIn(cur, seen, f'bucle evolutivo desde {num}')
                seen.add(cur)
                cur = self.byNum[cur][4]
                self.assertLessEqual(len(seen), 5, f'cadena demasiado larga desde {num}')

    def test_todos_los_tipos_tienen_color(self):
        for num, slug, display, tipo, *_ in self.DEX:
            self.assertIn(tipo, self.ACCENTS, f'{display}: tipo "{tipo}" sin color')

    def test_los_15_tipos_de_gen_1_tienen_orden_y_color(self):
        # el orden es el de la tabla de eficacias de battle.h: 15 tipos, sin repetir
        self.assertEqual(len(self.ORDER), 15)
        self.assertEqual(len(set(self.ORDER)), 15)
        self.assertEqual(set(self.ORDER), set(self.ACCENTS), 'cada tipo con su color y su sitio en la tabla')

    def test_los_segundos_tipos_son_validos(self):
        for num, t2 in self.TYPE2.items():
            self.assertIn(num, self.byNum, f'segundo tipo para una especie inexistente: {num}')
            self.assertIn(t2, self.ORDER, f'{self.byNum[num][2]}: tipo "{t2}" desconocido')
            self.assertNotEqual(t2, self.byNum[num][3], f'{self.byNum[num][2]}: el mismo tipo dos veces')
        # muestras de gen 1
        self.assertEqual(self.TYPE2[6], 'volador')    # Charizard
        self.assertEqual(self.TYPE2[94], 'veneno')    # Gengar
        self.assertEqual(self.TYPE2[130], 'volador')  # Gyarados
        self.assertNotIn(25, self.TYPE2)              # Pikachu: electrico puro
        self.assertNotIn('volador', [row[3] for row in self.DEX], 'ningun gen 1 es volador puro')

    def test_la_forma_base_de_cada_linea(self):
        from gen_dex import line_bases
        base = line_bases()
        evolucionadas = {row[4] for row in self.DEX if row[4]} | {135, 136}
        for num in self.byNum:
            b = base[num]
            self.assertNotIn(b, evolucionadas, f'{self.byNum[num][2]}: su base {b} es una evolucion')
            self.assertEqual(base[b], b)
        self.assertEqual(base[6], 4)      # Charizard -> Charmander
        self.assertEqual(base[135], 133)  # Jolteon -> Eevee, aunque Eevee apunte a Vaporeon
        self.assertEqual(base[136], 133)
        self.assertEqual(base[150], 150)

    def test_los_colores_son_hex_de_6_digitos(self):
        for tipo, col in self.ACCENTS.items():
            self.assertRegex(col, r'^#[0-9a-fA-F]{6}$', f'color raro en {tipo}: {col}')

    def test_las_rarezas_apuntan_a_formas_base(self):
        evolucionadas = {row[4] for row in self.DEX if row[4]}
        for num in sorted(self.RARE | self.LEGENDARY):
            self.assertIn(num, self.byNum, f'rareza para una especie inexistente: {num}')
            self.assertNotIn(num, evolucionadas,
                             f'{self.byNum[num][2]} solo sale por evolucion: la rareza no le sirve')
        self.assertFalse(self.RARE & self.LEGENDARY, 'una especie no puede ser rara y legendaria')

    def test_los_iniciales_clasicos_existen(self):
        evolucionadas = {row[4] for row in self.DEX if row[4]}
        for num in self.CLASSIC:
            self.assertIn(num, self.byNum, f'inicial inexistente: {num}')
            self.assertNotIn(num, evolucionadas, f'{self.byNum[num][2]} no puede ser inicial')

    def test_hay_stats_base_de_las_151(self):
        for num in self.byNum:
            self.assertIn(num, self.STATS, f'faltan stats de {self.byNum[num][2]}')
            hp, atk, dfn, spe = self.STATS[num]
            for v in (hp, atk, dfn, spe):
                self.assertTrue(0 < v <= 255, f'stat fuera de rango en {self.byNum[num][2]}: {v}')

    def test_los_nombres_traducidos_cuadran_con_su_fuente(self):
        # Los latinos se pintan con la fuente CP437 (un byte por caracter, sin
        # minusculas en los nombres); el japones y el coreano con una fuente
        # U8g2 y van en UTF-8, asi que ni son ASCII ni tienen mayusculas que
        # comprobar.
        LATINOS, CJK = ('fr', 'de'), ('ja', 'ko')
        for num, langs in self.NAMES.items():
            self.assertIn(num, self.byNum, f'nombre traducido de una especie inexistente: {num}')
            for lang, name in langs.items():
                self.assertIn(lang, LATINOS + CJK, f'idioma inesperado: {lang}')
                self.assertTrue(name, f'{num} {lang} vacio')
                if lang in LATINOS:
                    self.assertTrue(name.isascii(), f'{num} {lang} no es ASCII: {name}')
                    self.assertEqual(name, name.upper(), f'{num} {lang} lleva minusculas: {name}')
                    self.assertLessEqual(len(name), 12, f'{num} {lang} no cabe en pantalla: {name}')
                else:
                    # el limite util son CARACTERES, no bytes: en UTF-8 cada kana o silaba hangul ocupa 3
                    self.assertLessEqual(len(name), 8, f'{num} {lang} no cabe en pantalla: {name}')


class TestGeneratedFiles(unittest.TestCase):
    """dex.h esta generado: si no coincide con su generador, alguien lo edito a mano."""

    def test_gen_dex_reproduce_dex_h(self):
        tmp = tempfile.mkdtemp(prefix='tamapoke-gen-')
        try:
            shutil.copytree(TOOLS, os.path.join(tmp, 'tools'))
            r = subprocess.run([sys.executable, os.path.join(tmp, 'tools', 'gen_dex.py')],
                               capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, f'gen_dex.py fallo:\n{r.stderr}')
            generated = os.path.join(tmp, 'dex.h')
            self.assertTrue(os.path.exists(generated), 'gen_dex.py no escribio dex.h')
            with open(generated) as a, open(os.path.join(ROOT, 'dex.h')) as b:
                self.assertEqual(a.read(), b.read(),
                                 'dex.h no coincide con lo que genera tools/gen_dex.py '
                                 '(vuelve a generarlo en vez de editarlo a mano)')
        finally:
            shutil.rmtree(tmp, ignore_errors=True)

    def test_los_ficheros_generados_avisan_de_que_lo_son(self):
        for name in ('dex.h', 'species.h'):
            with open(os.path.join(ROOT, name)) as fh:
                head = fh.read(400)
            self.assertIn('GENERADO', head, f'{name} deberia decir que esta generado')


def _tpk2_records(buf):
    """(nActs, [(id, w, h, nf, contado)], fin) de un .bin TPK2 (tools/pack_pmd.py)."""
    nacts = buf[4]
    (palcount,) = struct.unpack_from('<H', buf, 5)
    p = 7 + palcount * 2
    recs = []
    while p + 4 <= len(buf):
        aid, w, h, nf = buf[p:p + 4]
        recs.append((aid, w, h, nf, len(recs) < nacts))
        p += 4 + nf * 2 + w * h * nf
    return nacts, recs, p


class TestPmdSprites(unittest.TestCase):
    """Los sprites de la SD (tools/sdcard/mons) los lee sdmon.cpp, tambien el firmware viejo."""

    def test_los_bin_se_leen_hasta_el_final(self):
        mons = os.path.join(TOOLS, 'sdcard', 'mons')
        files = sorted(f for f in os.listdir(mons) if re.fullmatch(r'ps?\d{3}\.bin', f))
        self.assertEqual(len(files), 302)
        for name in files:
            with self.subTest(sprite=name):
                with open(os.path.join(mons, name), 'rb') as fh:
                    buf = fh.read()
                self.assertEqual(buf[:4], b'TPK2')
                self.assertLessEqual(len(buf), 3 * 1024 * 1024)  # tope de PmdMon::load
                nacts, recs, end = _tpk2_records(buf)
                self.assertEqual(end, len(buf), 'el ultimo registro no acaba justo al final')
                ids = [r[0] for r in recs]
                self.assertEqual(len(ids), len(set(ids)), 'accion repetida')
                self.assertIn(0, ids)  # Idle
                for aid, w, h, nf, counted in recs:
                    self.assertTrue(w and h and 1 <= nf <= 24)
                    # el firmware <= v1.68 rechaza ids >= 12: solo pueden ir en la cola sin contar
                    self.assertEqual(counted, aid < 12, f'accion {aid} mal colocada')
                    self.assertLess(aid, 14)  # PMD_NACTS en sdmon.h


class TestSketchSanity(unittest.TestCase):
    """Comprobaciones baratas sobre el .ino que no necesitan compilarlo."""

    @classmethod
    def setUpClass(cls):
        with open(os.path.join(ROOT, 'TamaPoke.ino')) as fh:
            cls.ino = fh.read()

    def test_el_sketch_no_lleva_caracteres_no_ascii_en_literales_de_pantalla(self):
        # los comentarios pueden llevar lo que quieran; los literales, no:
        # la fuente GFX es ASCII y un acento sale como basura
        for lineno, line in enumerate(self.ino.splitlines(), 1):
            code = line.split('//')[0]
            for chunk in code.split('"')[1::2]:
                self.assertTrue(chunk.isascii(),
                                f'TamaPoke.ino:{lineno}: literal no ASCII: {chunk!r}')

    def test_las_llaves_estan_equilibradas(self):
        self.assertEqual(self.ino.count('{'), self.ino.count('}'),
                         'llaves descompensadas en TamaPoke.ino')


class TestImuReplay(unittest.TestCase):
    """imu_replay.py lee lo que saca IMULOG tal cual, sin limpiarlo a mano."""

    def test_lee_una_captura_de_imulog_con_su_marca_de_fin(self):
        import imu_replay
        fd, path = tempfile.mkstemp(suffix='.csv')
        try:
            with os.fdopen(fd, 'w') as fh:
                fh.write('t_ms,ax_mg,ay_mg,az_mg,steps\n'
                         '42044,-49,-32,1020,0\n'
                         'cargado /mons/ps007.bin (106 KB)\n'
                         '42061,-50,-31,1016,0\n'
                         '[imulog] end\n')
            self.assertEqual(imu_replay.load(path),
                             [(42.044, -0.049, -0.032, 1.02), (42.061, -0.05, -0.031, 1.016)])
        finally:
            os.remove(path)



class TestBuffersDeTexto(unittest.TestCase):
    """Cada snprintf(buf, sizeof(buf), T(S_x), ...) cabe en su buffer en TODOS los idiomas.

    Los buffers se dimensionaron cuando toda cadena era de un byte por
    caracter. En UTF-8 un kana o una silaba hangul ocupan 3, y snprintf corta
    en silencio: a veces se pierden cifras ("きろく 2" por "きろく 219") y a
    veces corta a mitad de un caracter, que la fuente ya no sabe dibujar. No lo
    avisa ni el compilador ni la pantalla, asi que se comprueba aqui con el
    peor caso de cada argumento segun su tipo:

      %u / %d  -> 5 cifras (todos los argumentos son de 16 bits o menos)
      %lu      -> 10 cifras
      %s       -> lo que declare ARGS_S (nombre, apodo, baya, objeto...); un %s sin declarar hace fallar el
                  test, para que quien lo anada piense cuanto puede ocupar
    """

    ARGS_S = {  # que puede llegar por cada %s, en orden
        'S_NAME_FMT': ('estrella', 'nombre_o_apodo'),
        'S_RELEASE_FMT': ('nombre',),
        'S_INFO_FMT': ('baya',),
        'S_FAREWELL_BTN': ('nombre_o_apodo',),
        'S_DYING_FMT': ('nombre_o_apodo',),  # v1.60 (S_RUNAWAY_BTN ya no tiene %s)
        'S_USE_FMT': ('objeto',),
        'S_BUY_FMT': ('objeto',),
        # la caja (v1.37): S_NOW_FMT tambien recibe "HUEVO", mas corto que cualquier nombre
        'S_STORE_Q_FMT': ('nombre_o_apodo',),
        'S_WAKE_Q_FMT': ('nombre_o_apodo',),
        'S_SWAP_NOTE_FMT': ('nombre_o_apodo',),
        'S_NOW_FMT': ('nombre_o_apodo',),
        'S_GO_FMT': ('nombre_o_apodo',),  # estadios: el relevo de un aliado
    }
    CIFRAS = {'u': 5, 'd': 5, 'lu': 10}
    ESPEC = re.compile(r'%0?\d*(lu|u|d|s)')

    @classmethod
    def setUpClass(cls):
        import test_i18n_formats as fmt
        from dex_data import DEX
        from dex_names import LOCAL_NAMES

        def leer(nombre):
            with open(os.path.join(ROOT, nombre), encoding='utf-8') as fh:
                return fh.read()

        cls.LANGS = fmt.LANGS
        cuerpo = re.search(r'enum\s+StrId[^{]*\{(.*?)\}', leer('i18n.h'), re.S).group(1)
        ids = re.findall(r'\b(S_[A-Z0-9_]+)\b', cuerpo)
        filas = fmt.extract_table(leer('i18n.cpp'), 'STRINGS', len(cls.LANGS))
        cls.LIT = {sid: [fila[k] for fila in filas] for k, sid in enumerate(ids)}
        cls.ino = leer('TamaPoke.ino').splitlines()
        cls.APODO = int(re.search(r'char\s+nick\[(\d+)\]', leer('pet.h')).group(1)) - 1
        base = max(len(row[2].encode()) for row in DEX)
        cls.NOMBRE = {}
        for lang in cls.LANGS:
            propios = [n[lang.lower()] for n in LOCAL_NAMES.values() if lang.lower() in n]
            cls.NOMBRE[lang] = max([base] + [len(n.encode('utf-8')) for n in propios])

    @staticmethod
    def bytes_literal(literal):
        """Bytes que ocupa en memoria un literal C escrito como en el fuente."""
        cuerpo = literal[1:-1].encode('utf-8')
        n = i = 0
        while i < len(cuerpo):
            if cuerpo[i] == 0x5C:  # barra invertida: \ooo octal, o \n \" \\ ...
                j = i + 1
                while j < len(cuerpo) and j < i + 4 and 0x30 <= cuerpo[j] <= 0x37:
                    j += 1
                i = j if j > i + 1 else i + 2
            else:
                i += 1
            n += 1
        return n

    def peor_caso(self, sid, li):
        lang, lit = self.LANGS[li], self.LIT[sid][li]
        total = self.bytes_literal(lit) + 1  # + terminador
        pendientes = list(self.ARGS_S.get(sid, ()))
        for m in self.ESPEC.finditer(lit):
            total -= len(m.group(0))
            if m.group(1) != 's':
                total += self.CIFRAS[m.group(1)]
                continue
            self.assertTrue(pendientes, f'{sid}: tiene un %s sin declarar en ARGS_S')
            arg = pendientes.pop(0)
            if arg == 'estrella':
                total += 1
            elif arg == 'nombre':
                total += self.NOMBRE[lang]
            elif arg == 'nombre_o_apodo':
                total += max(self.NOMBRE[lang], self.APODO)
            elif arg == 'baya':
                total += max(self.bytes_literal(self.LIT[b][li]) for b in
                             ('S_BERRY_UNK', 'S_BERRY_RED', 'S_BERRY_BLUE', 'S_BERRY_GREEN'))
            elif arg == 'objeto':  # nombre de un objeto de la mochila
                total += max(self.bytes_literal(self.LIT[o][li]) for o in self.LIT if o.startswith('S_IT_'))
        return total

    def test_cada_texto_formateado_cabe_en_su_buffer_en_todos_los_idiomas(self):
        llamada = re.compile(r'snprintf\((\w+),\s*sizeof\(\1\),\s*T\((S_[A-Z0-9_]+)\)')
        revisadas = 0
        for n, linea in enumerate(self.ino, 1):
            m = llamada.search(linea.split('//')[0])
            if not m:
                continue
            var, sid = m.groups()
            tam = next((int(d.group(1)) for k in range(n - 2, -1, -1)
                        for d in [re.search(r'\bchar\s+' + var + r'\s*\[(\d+)\]', self.ino[k])]
                        if d), None)
            self.assertIsNotNone(tam, f'TamaPoke.ino:{n}: no encuentro la declaracion de {var}')
            revisadas += 1
            for li, lang in enumerate(self.LANGS):
                with self.subTest(linea=n, id=sid, idioma=lang):
                    peor = self.peor_caso(sid, li)
                    self.assertLessEqual(
                        peor, tam,
                        f'TamaPoke.ino:{n}: {var}[{tam}] con {sid} en {lang} necesita {peor} bytes')
        self.assertGreater(revisadas, 10, 'el patron de snprintf ya no encuentra las llamadas')


def _pil_ok():
    try:
        from PIL import Image
        Image.new('RGB', (4, 4)).quantize(colors=4)
        return True
    except Exception:
        return False


class TestPackYard(unittest.TestCase):
    """tools/pack_yard.py (v2.0): mapas de texto -> TPY1. Usa un paquete de baldosas falso de 4x4
    baldosas lisas (el de verdad no esta en git)."""

    LEGEND = '\n'.join([
        'pack = fake',
        'sheet = sheet.png',
        'autotiles = set.yy',
        'g = 0,0 1,0  weights 3 1      # hierba',
        'w = autotile "Water" over g !  # agua con bordes',
        'R = stamp 2x2 2,2 !           # roca',
        'f = 3,0                       # flores (se pisan)',
        'u = 3,1 !                     # tocon',
    ]) + '\n'

    def setUp(self):
        import pack_yard
        self.py = pack_yard
        self.tmp = tempfile.mkdtemp()
        os.makedirs(os.path.join(self.tmp, 'legends'))
        os.makedirs(os.path.join(self.tmp, 'maps'))
        self.write('legends/fake.txt', self.LEGEND)
        fake = os.path.join(self.tmp, 'tilesets', 'fake')
        os.makedirs(fake)
        # juego de bordes: la posicion es la mascara de esquinas de fondo; baldosa 0 = vacia
        tiles = [4 * 3 + 0] + [4 * 3 + 1] * 14 + [0]
        with open(os.path.join(fake, 'set.yy'), 'w') as fh:
            fh.write('{"out_columns": 4, "autoTileSets": [{"name": "Water", "tiles": %s,},],}' % tiles)
        if _pil_ok():
            from PIL import Image
            sheet = Image.new('RGBA', (64, 64))
            for i in range(16):
                sheet.paste((i * 15, 255 - i * 15, (i * 40) % 256, 255), ((i % 4) * 16, (i // 4) * 16, (i % 4) * 16 + 16, (i // 4) * 16 + 16))
            sheet.save(os.path.join(fake, 'sheet.png'))

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def write(self, rel, text):
        with open(os.path.join(self.tmp, rel), 'w') as fh:
            fh.write(text)

    def mapa(self, ground=None, objects=None, head='name: Test\nlegend: fake\nseed: 1\n'):
        ground = ground or ['g' * 15] * 15
        objects = objects or ['.' * 15] * 15
        text = head + 'ground:\n' + '\n'.join(ground) + '\nobjects:\n' + '\n'.join(objects) + '\n'
        self.write('maps/01-test.txt', text)
        return os.path.join(self.tmp, 'maps', '01-test.txt')

    def falla(self, patron, path=None):
        path = path or os.path.join(self.tmp, 'maps', '01-test.txt')
        with self.assertRaisesRegex(self.py.MapError, patron):
            self.py.compose(path, self.tmp, os.path.join(self.tmp, 'tilesets'))

    def con_objeto(self, fila, texto):
        objs = ['.' * 15] * 15
        objs[fila] = texto
        return objs

    # ---- errores del mapa y de la leyenda (no necesitan Pillow hasta componer) ----
    def test_fila_de_ancho_equivocado(self):
        g = ['g' * 15] * 15
        g[3] = 'g' * 14
        path = self.mapa(ground=g)
        with self.assertRaisesRegex(self.py.MapError, r'01-test.txt:8: ground row has 14 characters'):
            self.py.parse_map(path)

    def test_faltan_filas(self):
        path = self.mapa(ground=['g' * 15] * 14)
        with self.assertRaisesRegex(self.py.MapError, 'ground layer has 14 rows'):
            self.py.parse_map(path)

    def test_falta_el_nombre(self):
        path = self.mapa(head='legend: fake\n')
        with self.assertRaisesRegex(self.py.MapError, 'missing "name'):
            self.py.parse_map(path)

    def test_leyenda_sin_hoja(self):
        self.write('legends/bad.txt', 'pack = fake\ng = 0,0\n')
        with self.assertRaisesRegex(self.py.MapError, 'missing "sheet'):
            self.py.parse_legend(os.path.join(self.tmp, 'legends', 'bad.txt'))

    def test_leyenda_pesos_que_no_cuadran(self):
        self.write('legends/bad.txt', 'pack = fake\nsheet = s.png\ng = 0,0 1,0 weights 1\n')
        with self.assertRaisesRegex(self.py.MapError, r'bad.txt:3: 2 tiles but 1 weights'):
            self.py.parse_legend(os.path.join(self.tmp, 'legends', 'bad.txt'))

    def test_leyenda_clave_larga(self):
        self.write('legends/bad.txt', 'pack = fake\nsheet = s.png\ngg = 0,0\n')
        with self.assertRaisesRegex(self.py.MapError, 'one character'):
            self.py.parse_legend(os.path.join(self.tmp, 'legends', 'bad.txt'))

    def test_falta_el_paquete_y_dice_que_hacer(self):
        shutil.rmtree(os.path.join(self.tmp, 'tilesets'))
        self.mapa()
        self.falla('tile pack not found(.|\n)*not in git')

    def test_nombre_de_fichero(self):
        self.write('maps/meadow.txt', 'x')
        with self.assertRaisesRegex(self.py.MapError, 'NN-name.txt'):
            self.py.map_files(self.tmp, [])

    def test_mapa_pedido_que_no_existe(self):
        self.mapa()
        with self.assertRaisesRegex(self.py.MapError, 'no map numbered 07'):
            self.py.map_files(self.tmp, ['7'])

    # ---- errores al componer ----
    @unittest.skipUnless(_pil_ok(), 'Pillow no disponible')
    def test_caracter_desconocido(self):
        self.mapa(objects=self.con_objeto(2, '....Z..........'))
        self.falla(r'01-test.txt:23:5: unknown character .Z. in objects')

    @unittest.skipUnless(_pil_ok(), 'Pillow no disponible')
    def test_sello_fuera_de_la_rejilla(self):
        self.mapa(objects=self.con_objeto(14, '..............R'))
        self.falla("stamp 'R' \\(2x2\\) runs off the grid")

    @unittest.skipUnless(_pil_ok(), 'Pillow no disponible')
    def test_sellos_que_se_pisan(self):
        objs = ['.' * 15] * 15
        objs[3] = '...R...........'
        objs[4] = '....R..........'
        self.mapa(objects=objs)
        self.falla("stamp 'R' overlaps 'R'")

    @unittest.skipUnless(_pil_ok(), 'Pillow no disponible')
    def test_autotile_en_objetos(self):
        self.mapa(objects=self.con_objeto(0, 'w..............'))
        self.falla('autotile .w. only allowed in the ground layer')

    @unittest.skipUnless(_pil_ok(), 'Pillow no disponible')
    def test_sello_en_el_suelo(self):
        g = ['g' * 15] * 15
        g[5] = 'R' + 'g' * 14
        self.mapa(ground=g)
        self.falla('stamp .R. not allowed in the ground layer')

    @unittest.skipUnless(_pil_ok(), 'Pillow no disponible')
    def test_juego_de_bordes_desconocido(self):
        self.write('legends/fake.txt', self.LEGEND.replace('"Water"', '"Lava"'))
        g = ['g' * 15] * 15
        g[7] = 'gggggggwggggggg'
        self.mapa(ground=g)
        self.falla("autotile set 'Lava' not found")

    # ---- el fichero TPY1 ----
    @unittest.skipUnless(_pil_ok(), 'Pillow no disponible')
    def test_tpy1_vuelve_a_dar_la_imagen_y_la_mascara(self):
        g = ['g' * 15] * 15
        g[7] = 'gggggggwggggggg'
        self.mapa(ground=g, objects=self.con_objeto(4, '....R.....f.u..'))
        meta, img, blocked = self.py.compose(os.path.join(self.tmp, 'maps', '01-test.txt'), self.tmp,
                                             os.path.join(self.tmp, 'tilesets'))
        self.assertEqual(meta['name'], 'Test')
        data, colours = self.py.encode_tpy1(img, blocked)
        self.assertEqual(data[:7], b'TPY1\x01\xf0\xf0')  # firma, version 1, 240x240
        pal, walk, px = self.py.decode_tpy1(data)
        self.assertEqual(len(px), 240 * 240)
        # cada pixel decodificado es el color original pasado a RGB565 (pocos colores: sin perdida)
        rgb = img.tobytes()
        for i in range(0, 240 * 240, 97):
            r, g_, b = rgb[3 * i:3 * i + 3]
            self.assertEqual(pal[px[i]], self.py.rgb565(r, g_, b), f'pixel {i}')

        def bit(x, y):
            k = y * 15 + x
            return (walk[k // 8] >> (k % 8)) & 1
        self.assertEqual(bit(4, 4), 1)   # roca (sello de 2x2 bloqueante)
        self.assertEqual(bit(5, 5), 1)
        self.assertEqual(bit(12, 4), 1)  # tocon
        self.assertEqual(bit(10, 4), 0)  # flores: se pisan
        self.assertEqual(bit(7, 7), 1)   # el agua maciza bloquea
        self.assertEqual(bit(7, 8), 0)   # su borde no
        self.assertEqual(bit(7, 1), 1)   # zona de la hora
        self.assertEqual(bit(7, 13), 1)  # zona del dock
        self.assertEqual(bit(0, 0), 1)   # fuera del circulo
        self.assertEqual(bit(9, 9), 0)   # hierba libre

    def test_tpy1_corto_o_con_otra_firma(self):
        with self.assertRaises(ValueError):
            self.py.decode_tpy1(b'TPY2' + bytes(100))
        with self.assertRaises(ValueError):
            self.py.decode_tpy1(b'TPY1\x01\xf0\xf0\x01' + bytes(10))


_REAL_TILESETS = os.environ.get('TAMAPOKE_TILESETS', '/home/ale/projects/TamaPoke/tools/yard/tilesets')


@unittest.skipUnless(_pil_ok(), 'Pillow no disponible')
@unittest.skipUnless(os.path.isdir(os.path.join(_REAL_TILESETS, 'sunnyside')),
                     'paquete Sunnyside no instalado (TAMAPOKE_TILESETS)')
class TestYardMapsRealPack(unittest.TestCase):
    """Empaqueta cada mapa de tools/yard/maps con el paquete de baldosas de verdad (no esta en git:
    se salta si falta). Sin vistas previas: el resize de Pillow del sistema puede estar roto."""

    # celdas sueltas (fuera de la zona grande) que ya tenian los mapas anteriores a esta prueba;
    # los mapas nuevos no pueden tener ninguna
    KNOWN_POCKETS = {'01-meadow': 2, '02-quarry': 6}

    def regiones(self, walk):
        libres = {(x, y) for y in range(15) for x in range(15)
                  if not (walk[(y * 15 + x) // 8] >> ((y * 15 + x) % 8)) & 1}
        tamanos = []
        while libres:
            pila = [libres.pop()]
            n = 1
            while pila:
                x, y = pila.pop()
                for v in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                    if v in libres:
                        libres.remove(v)
                        pila.append(v)
                        n += 1
            tamanos.append(n)
        return sorted(tamanos, reverse=True)

    def test_todos_los_mapas(self):
        import pack_yard
        yard = os.path.join(ROOT, 'tools', 'yard')
        maps = pack_yard.map_files(yard, [])
        self.assertGreaterEqual(len(maps), 5)
        for mp in maps:
            with self.subTest(mapa=mp.stem):
                meta, img, blocked = pack_yard.compose(mp, yard, _REAL_TILESETS)
                data, colores = pack_yard.encode_tpy1(img, blocked)
                self.assertLessEqual(colores, 256)
                self.assertLessEqual(len(data), 60 * 1024)
                _, walk, _ = pack_yard.decode_tpy1(data)
                regiones = self.regiones(walk)
                self.assertGreaterEqual(regiones[0], 40, f'zona para pasear: {regiones}')
                self.assertLessEqual(sum(regiones[1:]), self.KNOWN_POCKETS.get(mp.stem, 0),
                                     f'celdas sueltas fuera de la zona grande: {regiones}')


if __name__ == '__main__':
    unittest.main(verbosity=2)
