# Tests

Bateria de tests que corre **en el PC, sin placa**: compila la logica pura del
firmware (`pet.cpp`, `i18n.cpp`, `dex.h`) contra unos shims de Arduino y la
ejecuta con un reloj y un `random()` falsos, asi que los resultados son
deterministas y tardan menos de un segundo.

```bash
./test/run_tests.sh           # todo
./test/run_tests.sh --asan    # ademas con AddressSanitizer + UBSan
```

O por partes:

```bash
cd test && make               # solo la logica del firmware
cd test && make FILTER=evolve # solo los tests con "evolve" en el nombre
cd test && make asan          # con sanitizers
python3 test/test_tools.py    # solo las herramientas de tools/
```

## Que se cubre

| Fichero | Que comprueba |
|---|---|
| `test_pet.cpp` | huevo y eleccion de inicial, comida, tick de estadisticas, sueno, descuidos, niveles, evolucion (incluida la rama de Eevee), stats de combate, entrenamiento y minijuego, racha y vinculo, medallas, ceremonias, progresion offline y guardado en NVS; pasos (premios con sus restos y topes, la meta, el cambio de dia), hallazgos, mochila, amuleto shiny y las reacciones al IMU (enfado, mareo, boca abajo), sin escrituras a flash inmediatas |
| `test_steps.cpp` | el contador de pasos (`steps.h`) sobre las capturas reales de `data/imu/` (los mismos conteos que el prototipo `tools/imu_replay.py`; quieta, agitada o sacada del bolsillo para mirarla no cuenta) y senales sinteticas: paseo limpio, rafaga corta, pausas, la vuelta de `millis()` |
| `test_motion.cpp` | sacudida, boca abajo e inclinacion (`motion.h`) sobre las capturas y senales sinteticas: las dos sacudidas grabadas se ven y ningun paseo ni carrera lo es; boca abajo a los 3 s y no antes; el lado de la inclinacion con los dos signos de eje |
| `test_battle.cpp` | el motor de combate (`battle.h`): eficacias de gen 1 (con dos tipos y sus rarezas), stats de la ficha con el nivel topado, cada golpe entre el 5 y el 25 % (30 % el especial), "no le afecta", el tope de 15 ataques, el orden de turno, la misma semilla da el mismo combate, el medidor del especial y su espera, el rival de practica, y un Monte Carlo de los 151x151 a niveles 5, 30 y 100 (7-15 ataques siempre, el mas fuerte gana casi siempre) |
| `test_box.cpp` | la caja (v1.37, `box.h` y los movimientos de `Pet`): el registro y su CRC (cualquier byte cambiado lo delata), guardar deja un huevo y no es un final, despertar intercambia en el mismo hueco, el huevo que espera vuelve tal cual (grietas, tirada shiny, sin gastar otro amuleto), 20 vueltas sin volver a tirar el huevo, hibernacion durante el tiempo apagado, vinculo y medallas de cada bicho, nada del jugador cambia, huecos danados, guardados de la v1.36; y un **corte de luz tras cada escritura** de cada movimiento (el shim deja de escribir tras n, `mockNvsCutAfter`): tras arrancar, cada bicho esta una vez y sin cambios; v1.39: guardar sin huevos deja la casa vacia, guardar sacando un huevo de la mochila (un corte entre el uid y la mochila deja uno de mas, nunca uno de menos) y despertar con la casa vacia, tambien con un corte tras cada escritura |
| `test_eggs.cpp` | huevos limitados (v1.39): 300 semanas de 5000 pasos al dia dan ~3 huevos a la semana (2 a 4 cada una), nunca antes de 8000 pasos ni despues de 15000, cuentan dormido y con un huevo en casa, el contador sobrevive a un reinicio, mochila llena, el huevo no se usa a mano; la casa vacia (ni eclosiona ni envejece, tampoco apagada), un final o guardar saca un huevo de la mochila (antes el que esperaba), el huevo encontrado con la casa vacia va a casa sin escribir en flash, guardados de la v1.38 |
| `test_ota.cpp` | actualizaciones por Wi-Fi (v1.42, `ota_policy.h` e `improv.h`): versiones (1.9 < 1.10, mal formadas nunca parecen nuevas), el manifiesto (bueno, con claves nuevas, falta un campo, otra placa, mas grande que la ranura, roto o cortado, SHA y url, notas fuera del ASCII), sin bajar de version salvo forzando, aviso de sprites nuevos, condiciones para instalar (50 % sin USB, ceremonia, minijuego), SHA-256 (vectores conocidos y en trozos), la descarga por pasos (buena, suma mala, un byte cambiado, 20 s sin datos, conexion cortada, fallo de escritura, de mas datos, cancelar en cada momento: **`end()` nunca antes de comprobar**) y el Improv por serie (paquetes mezclados con las ordenes de texto, suma mala, RPC mal formados, respuestas) |
| `test_ballgame.cpp` | el minijuego de la pokeball (`ballgame.h`): 1x con 1 punto, 2x con 80 y tope 3x; con 80 puntos la bola pasa la mitad de tiempo en el aire sobre el mismo arco; rebote en la pared redonda |
| `test_dex.cpp` | integridad de la Pokedex: nombres ASCII, cadenas evolutivas sin bucles, toda especie alcanzable, ninguna linea imposible de completar, limites de los buffers de la UI; los tipos (uno o dos, sin volador como principal) y la forma base de cada linea |
| `test_i18n.cpp` | los 6 idiomas completos, sin acentos (la fuente GFX es ASCII), y con los mismos `%u`/`%s` que le pasa el sketch en cada llamada |
| `test_tools.py` | los scripts de `tools/` compilan, los datos de la Pokedex son coherentes, `dex.h` sigue coincidiendo con lo que genera `tools/gen_dex.py`, e `imu_replay.py` lee la salida de `IMULOG` tal cual |

## Como esta montado

- `shim/Arduino.h` + `shim/Preferences.h` + `shim/shim.cpp`: lo minimo de Arduino
  que usan `pet.cpp` e `i18n.cpp`. El reloj (`millis()`) y `random()` son falsos
  y controlables; `Preferences` es un mapa en memoria que imita la NVS, asi que
  se puede simular un reinicio creando otro `Pet` y llamando a `begin()`.
- `framework.h` + `main.cpp`: micro framework propio, sin dependencias.
- Los tests marcados con `TEST_KNOWN_ISSUE` describen el comportamiento
  **correcto** de algo que hoy falla: no rompen la bateria, salen listados
  aparte, y avisan si algun dia empiezan a pasar (para poder quitar la marca).

## Que NO se cubre

Todo lo que necesita la placa: pantalla y tactil (`TamaPoke.ino`), audio
(`audio.cpp`), tarjeta SD (`sdmon.cpp`) y RTC/bateria (`rtcbat.cpp`). El
`.ino` tampoco se compila aqui: hace falta `arduino-cli` con el core de ESP32.
