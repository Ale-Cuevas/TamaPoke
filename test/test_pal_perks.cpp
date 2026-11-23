// Premios de los dos bichos juntos (v3.22, pal_perks.h, Pet::palPlayPerk / palCheerPerk / palSceneSeen): JOY con
// tope diario por bicho (uid + dia), el blob "pal" y la medalla Companeros de juego. Los numeros salen de tuning.h.
#include "framework.h"
#include "shim/Arduino.h"
#include "shim/Preferences.h"
#include "../pet.h"
#include "../pal_perks.h"

static const uint32_t PDAY = 22000;  // un dia cualquiera (numero de dia unix)

// bicho recien nacido (Charmander) en el hueco 0, reloj a PDAY, NVS limpia
static void pBorn(Pet &p) {
  mockClearForcedRandom();
  mockNvsReset();
  mockSetMillis(0);
  mockSerialSilence(true);
  p.begin();
  if (p.awaitingStarter()) p.chooseStarter(4);
  p.eggTap();
  p.eggTap();
  p.eggTap();
  p.setClock(PDAY * 86400 + 10 * 3600);
}

// el hueco 1 con otro bicho nacido (copia del 0 con otro uid)
static void pSecond(Pet &p) {
  PetLife &o = p.slotRef(1);
  o = p.home(0);
  o.uid = p.uid + 77;
  o.joy = 50;
}

TEST(pal_perks, el_juego_da_joy_y_se_topa) {
  Pet p;
  pBorn(p);
  for (uint8_t i = 0; i < PAL_PLAY_PAY_DAY; i++) {
    p.joy = 10;
    CHECK_EQ(p.palPlayPerk(0), (uint8_t)PAL_PLAY_JOY);
    CHECK_EQ(p.joy, (uint8_t)(10 + PAL_PLAY_JOY));
  }
  p.joy = 10;
  CHECK_EQ(p.palPlayPerk(0), (uint8_t)0);  // ya cobro hoy
  CHECK_EQ(p.joy, (uint8_t)10);
  // el animo tiene su propio contador
  for (uint8_t i = 0; i < PAL_CHEER_DAY; i++) {
    p.joy = 10;
    CHECK_EQ(p.palCheerPerk(0), (uint8_t)PAL_CHEER_JOY);
    CHECK_EQ(p.joy, (uint8_t)(10 + PAL_CHEER_JOY));
  }
  p.joy = 10;
  CHECK_EQ(p.palCheerPerk(0), (uint8_t)0);
  CHECK_EQ(p.palSlotOf(0).cheersPaid, (uint8_t)PAL_CHEER_DAY);
  CHECK_EQ(p.palSlotOf(0).playsPaid, (uint8_t)PAL_PLAY_PAY_DAY);
}

TEST(pal_perks, el_joy_se_topa_en_100_y_devuelve_lo_dado) {
  Pet p;
  pBorn(p);
  p.joy = 100;
  CHECK_EQ(p.palPlayPerk(0), (uint8_t)0);  // cuenta como cobrado aunque no haya sitio
  p.joy = 100 - 1;
  CHECK_EQ(p.palPlayPerk(0), (uint8_t)1);
  CHECK_EQ(p.joy, (uint8_t)100);
  CHECK(p.savePending());
}

TEST(pal_perks, dia_nuevo_y_bicho_nuevo_reinician) {
  Pet p;
  pBorn(p);
  for (int i = 0; i < 10; i++) p.palPlayPerk(0);
  p.joy = 0;
  CHECK_EQ(p.palPlayPerk(0), (uint8_t)0);
  p.setClock((PDAY + 1) * 86400 + 10 * 3600);  // dia siguiente
  p.joy = 0;
  CHECK_EQ(p.palPlayPerk(0), (uint8_t)PAL_PLAY_JOY);
  CHECK_EQ(p.palSlotOf(0).playsPaid, (uint8_t)1);
  // otro bicho en el hueco (otro uid): empieza de cero el mismo dia
  for (int i = 0; i < 10; i++) p.palPlayPerk(0);
  p.uid += 5;
  p.joy = 0;
  CHECK_EQ(p.palPlayPerk(0), (uint8_t)PAL_PLAY_JOY);
  CHECK_EQ(p.palSlotOf(0).playsPaid, (uint8_t)1);
}

TEST(pal_perks, dormido_huevo_vacio_o_sin_reloj_no_dan) {
  Pet p;
  pBorn(p);
  pSecond(p);
  p.sleeping = true;
  p.joy = 10;
  CHECK_EQ(p.palPlayPerk(0), (uint8_t)0);
  CHECK_EQ(p.palCheerPerk(0), (uint8_t)0);
  p.sleeping = false;
  p.slotRef(1).sleeping = true;
  CHECK_EQ(p.palPlayPerk(1), (uint8_t)0);
  p.slotRef(1).sleeping = false;
  p.slotRef(1).vacant = true;
  CHECK_EQ(p.palPlayPerk(1), (uint8_t)0);
  p.slotRef(1).vacant = false;
  p.slotRef(1).speciesId = -1;  // huevo
  CHECK_EQ(p.palPlayPerk(1), (uint8_t)0);
  CHECK_EQ(p.palCheerPerk(1), (uint8_t)0);
  // nada se cobro
  CHECK_EQ(p.palSlotOf(0).playsPaid, (uint8_t)0);
  CHECK_EQ(p.palSlotOf(1).playsPaid, (uint8_t)0);
  // sin reloj
  Pet q;
  mockNvsReset();
  q.begin();
  if (q.awaitingStarter()) q.chooseStarter(4);
  q.eggTap();
  q.eggTap();
  q.eggTap();
  q.lastSeenEpoch = 0;
  q.joy = 10;
  CHECK_EQ(q.dayNumber(), (uint32_t)0);
  CHECK_EQ(q.palPlayPerk(0), (uint8_t)0);
}

TEST(pal_perks, el_hueco_1_cobra_aparte_y_sin_mover_el_foco) {
  Pet p;
  pBorn(p);
  pSecond(p);
  p.slotRef(1).joy = 40;
  uint8_t joy0 = p.joy;
  CHECK_EQ(p.palPlayPerk(1), (uint8_t)PAL_PLAY_JOY);
  CHECK_EQ(p.home(1).joy, (uint8_t)(40 + PAL_PLAY_JOY));
  CHECK_EQ(p.joy, joy0);  // el del foco no se toco
  CHECK_EQ(p.focusSlot, (uint8_t)0);
  CHECK_EQ(p.palSlotOf(1).playsPaid, (uint8_t)1);
  CHECK_EQ(p.palSlotOf(0).playsPaid, (uint8_t)0);
  CHECK_EQ(p.palSlotOf(1).uid, p.home(1).uid);
  // y con el foco en el 1 sigue cobrando el mismo hueco
  p.focus(1);
  CHECK_EQ(p.palCheerPerk(1), (uint8_t)PAL_CHEER_JOY);
  CHECK_EQ(p.palSlotOf(1).cheersPaid, (uint8_t)1);
}

TEST(pal_perks, el_blob_pal_vuelve_y_falta_o_danado_no_cobra_nada) {
  {
    Pet p;
    pBorn(p);
    pSecond(p);
    p.palPlayPerk(0);
    p.palPlayPerk(0);
    p.palCheerPerk(1);
    p.setClock(p.lastSeenEpoch);  // guarda
  }
  {
    Pet q;
    q.begin();
    CHECK_EQ(q.palSlotOf(0).playsPaid, (uint8_t)2);
    CHECK_EQ(q.palSlotOf(0).day, PDAY);
    CHECK_EQ(q.palSlotOf(1).cheersPaid, (uint8_t)1);
    CHECK(q.palSlotOf(0).uid != 0);
  }
  {  // danado: CRC malo
    Preferences x;
    x.begin("tamapoke", false);
    PalSave s;
    CHECK_EQ(x.getBytes("pal", &s, sizeof(s)), sizeof(s));
    s.slot[0].playsPaid = 0;  // sin resellar
    x.putBytes("pal", &s, sizeof(s));
  }
  {
    Pet q;
    q.begin();
    CHECK_EQ(q.palSlotOf(0).playsPaid, (uint8_t)0);
    CHECK_EQ(q.palSlotOf(1).cheersPaid, (uint8_t)0);
  }
  {  // cortado y ausente
    Preferences x;
    x.begin("tamapoke", false);
    PalSave s;
    memset(&s, 0, sizeof(s));
    x.putBytes("pal", &s, 5);
    Pet q;
    q.begin();
    CHECK_EQ(q.palSlotOf(0).playsPaid, (uint8_t)0);
    x.remove("pal");
    Pet r;
    r.begin();
    CHECK_EQ(r.palSlotOf(0).playsPaid, (uint8_t)0);
  }
}

TEST(pal_perks, un_fallo_al_escribir_pal_se_reintenta) {
  Pet p;
  pBorn(p);
  mockNvsFailKey("pal");
  p.palPlayPerk(0);
  p.setClock(p.lastSeenEpoch);
  CHECK(p.savePending());  // se reintenta
  mockNvsFailKey(nullptr);
  p.setClock(p.lastSeenEpoch);
  Pet q;
  q.begin();
  CHECK_EQ(q.palSlotOf(0).playsPaid, (uint8_t)1);
}

TEST(pal_perks, companeros_cuenta_y_paga_sus_tramos) {
  Pet p;
  pBorn(p);
  CHECK_EQ(p.medalValue(MD_PLAYMATES), (uint32_t)0);
  uint16_t c0 = p.coins;
  for (uint32_t i = 0; i < MEDAL_T[MD_PLAYMATES][0]; i++) p.palSceneSeen();
  p.medalsUpdate();
  CHECK_EQ(p.medalValue(MD_PLAYMATES), MEDAL_T[MD_PLAYMATES][0]);
  CHECK_EQ(p.medalTierPaid(MD_PLAYMATES), (uint8_t)1);
  CHECK_EQ((uint32_t)p.coins, (uint32_t)c0 + MEDAL_COINS[0]);
  for (uint32_t i = MEDAL_T[MD_PLAYMATES][0]; i < MEDAL_T[MD_PLAYMATES][1]; i++) p.palSceneSeen();
  p.medalsUpdate();
  CHECK_EQ(p.medalTierPaid(MD_PLAYMATES), (uint8_t)2);
  CHECK_EQ((uint32_t)p.coins, (uint32_t)c0 + MEDAL_COINS[0] + MEDAL_COINS[1]);
  CHECK(p.savePending());
  CHECK_EQ((int)medalSource(MD_PLAYMATES), (int)MSRC_EVENT);
}
