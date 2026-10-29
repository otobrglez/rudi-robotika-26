#include <MD_MAX72xx.h>
#include <SPI.h>
#define IR_USE_AVR_TIMER5   // daljinec na časovniku 5, ker tone() za piskač rabi časovnik 2
#include <IRremote.hpp>

#define TIP   MD_MAX72XX::GENERIC_HW
#define DIN   7
#define CLK   6
#define CS    5
#define IR_PIN 2   // Y (signal) na IR sprejemniku
#define PISKAC 8   // + nožica pasivnega piskača

MD_MAX72XX matrika = MD_MAX72XX(TIP, DIN, CLK, CS, 1);

// ---------- tipke na daljincu ELEGOO ----------
const byte T_GOR    = 0x46;  // VOL+  = zavrti kos
const byte T_DOL    = 0x15;  // VOL-  = hitreje dol
const byte T_LEVO   = 0x44;  // |<<   = levo
const byte T_DESNO  = 0x43;  // >>|   = desno
const byte T_SREDINA = 0x40; // >||   (v tej igri je ne uporabljamo)
const byte T_2 = 0x18, T_8 = 0x52, T_4 = 0x08, T_6 = 0x5A;  // tudi številke

// ---------- nastavitve ----------
const int ZAC_HITROST = 700;  // ms med padci na začetku
const int POSPESEK    = 40;   // za toliko ms hitreje po vsaki polni vrstici
const int NAJ_HITROST = 200;  // hitreje od tega ne gre
const int FAZ_KUP     = 3;    // zloženi kup sveti 1 od 3 ciklov, padajoči kos polno

// ---------- kosi ----------
// vsak kos ima do 3 kockice, zapisane kot zamik od središča (x, y)
// majhni kosi, ker je plošča samo 8 x 8
const int ST_KOSOV = 4;
const int VELIKOST[ST_KOSOV] = { 1, 2, 3, 3 };
const int OBLIKA[ST_KOSOV][3][2] = {
  { {0, 0}, {0, 0}, {0, 0} },    // pika
  { {0, 0}, {1, 0}, {0, 0} },    // dvojček
  { {-1, 0}, {0, 0}, {1, 0} },   // palica
  { {0, 0}, {1, 0}, {0, 1} },    // kotiček
};

// ---------- stanje igre ----------
bool kup[8][8];              // zložene kockice [y][x]
int kosX[3], kosY[3];        // zamiki trenutnega kosa
int velikost;
int sredX, sredY;            // kje je središče kosa
int vrstic;                  // koliko polnih vrstic je igralec počistil
int hitrost;
unsigned long naslednjiPadec;

// ---------- risanje (kot pri kači) ----------
byte obrniBite(byte b) {
  byte r = 0;
  for (int i = 0; i < 8; i++) {
    r = (r << 1) | (b & 1);
    b >>= 1;
  }
  return r;
}

void poslji(byte buf[8]) {
  for (int vrstica = 0; vrstica < 8; vrstica++) {
    matrika.setRow(0, 7 - vrstica, obrniBite(buf[vrstica]));
  }
  matrika.update();
}

void prizgi(byte buf[8], int x, int y) {
  if (y < 0) return;                  // nad ploščo se ne vidi
  buf[y] |= (B10000000 >> x);
}

// ena slika: kup je zatemnjen, padajoči kos sveti polno
int faza = 0;
void narisi() {
  byte buf[8] = { 0 };
  if (faza == 0)
    for (int y = 0; y < 8; y++)
      for (int x = 0; x < 8; x++)
        if (kup[y][x]) prizgi(buf, x, y);
  for (int i = 0; i < velikost; i++) prizgi(buf, sredX + kosX[i], sredY + kosY[i]);
  poslji(buf);
  faza = (faza + 1) % FAZ_KUP;
}

// ---------- zvok ----------
// zaigra ton s frekvenco hz (Hz) dolžine ms in počaka, da se konča
void pisk(int hz, int ms) {
  tone(PISKAC, hz, ms);
  delay(ms + 20);
}

void zvokVrstica() {          // kratek vesel pisk navzgor
  pisk(660, 70);
  pisk(880, 70);
  pisk(1320, 120);
}

void zvokKonec() {            // žalostna melodija navzdol
  pisk(523, 200);
  pisk(392, 200);
  pisk(330, 200);
  pisk(262, 500);
}

// ---------- pomožno ----------
// ali lahko kos z zamiki (zx, zy) stoji na mestu (sx, sy)
bool prostoZa(int zx[], int zy[], int sx, int sy) {
  for (int i = 0; i < velikost; i++) {
    int x = sx + zx[i], y = sy + zy[i];
    if (x < 0 || x > 7 || y > 7) return false;   // stena ali tla
    if (y >= 0 && kup[y][x]) return false;        // zložene kockice
  }
  return true;
}

// vrne false, če za nov kos ni več prostora (konec igre)
bool novKos() {
  int k = random(ST_KOSOV);
  velikost = VELIKOST[k];
  for (int i = 0; i < velikost; i++) {
    kosX[i] = OBLIKA[k][i][0];
    kosY[i] = OBLIKA[k][i][1];
  }
  sredX = 3; sredY = 0;
  return prostoZa(kosX, kosY, sredX, sredY);
}

void premakni(int dx) {
  if (prostoZa(kosX, kosY, sredX + dx, sredY)) sredX += dx;
}

void zavrti() {
  int nx[3], ny[3];
  for (int i = 0; i < velikost; i++) {   // zasuk za 90 stopinj: (x, y) -> (-y, x)
    nx[i] = -kosY[i];
    ny[i] =  kosX[i];
  }
  if (prostoZa(nx, ny, sredX, sredY))
    for (int i = 0; i < velikost; i++) { kosX[i] = nx[i]; kosY[i] = ny[i]; }
}

void utripniVrstico(int y) {
  for (int i = 0; i < 3; i++) {
    byte buf[8] = { 0 };
    for (int yy = 0; yy < 8; yy++)
      for (int x = 0; x < 8; x++)
        if (kup[yy][x] && (yy != y || i % 2 == 1)) prizgi(buf, x, yy);
    poslji(buf);
    delay(100);
  }
}

void pocistiVrstice() {
  for (int y = 7; y >= 0; y--) {
    bool polna = true;
    for (int x = 0; x < 8; x++) if (!kup[y][x]) polna = false;
    if (!polna) continue;

    zvokVrstica();
    utripniVrstico(y);
    for (int yy = y; yy > 0; yy--)          // vse nad njo pade za eno dol
      for (int x = 0; x < 8; x++) kup[yy][x] = kup[yy - 1][x];
    for (int x = 0; x < 8; x++) kup[0][x] = false;

    vrstic++;
    hitrost = max(NAJ_HITROST, hitrost - POSPESEK);
    Serial.print("Polna vrstica! Skupaj: ");
    Serial.println(vrstic);
    y++;                                    // isto vrstico preveri še enkrat
  }
}

// ---------- igra ----------
void novaIgra() {
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 8; x++) kup[y][x] = false;
  vrstic = 0;
  hitrost = ZAC_HITROST;
  novKos();
  naslednjiPadec = millis() + hitrost;
  Serial.println("Nova igra!");
}

// številke 0-9, vsaka je velika 3 x 5 pik (vsaka vrstica so 3 biti)
const byte STEVKE[10][5] = {
  { B111, B101, B101, B101, B111 },  // 0
  { B010, B110, B010, B010, B111 },  // 1
  { B111, B001, B111, B100, B111 },  // 2
  { B111, B001, B111, B001, B111 },  // 3
  { B101, B101, B111, B001, B001 },  // 4
  { B111, B100, B111, B001, B111 },  // 5
  { B111, B100, B111, B101, B111 },  // 6
  { B111, B001, B010, B010, B010 },  // 7
  { B111, B101, B111, B101, B111 },  // 8
  { B111, B101, B111, B001, B111 },  // 9
};

void narisiStevko(byte buf[8], int stevka, int zx) {
  for (int y = 0; y < 5; y++)
    for (int x = 0; x < 3; x++)
      if (STEVKE[stevka][y] & (B100 >> x)) prizgi(buf, zx + x, y + 1);
}

// na matriki pokaže število (največ 99)
void pokaziStevilo(int n, int ms) {
  n = min(n, 99);
  byte buf[8] = { 0 };
  if (n < 10) {
    narisiStevko(buf, n, 2);           // ena številka na sredini
  } else {
    narisiStevko(buf, n / 10, 0);      // desetice
    narisiStevko(buf, n % 10, 4);      // enice
  }
  poslji(buf);
  delay(ms);
}

void konecIgre() {
  Serial.print("Konec igre. Vrstic: ");
  Serial.println(vrstic);
  zvokKonec();
  for (int i = 0; i < 3; i++) {
    matrika.control(MD_MAX72XX::TEST, true);
    delay(200);
    matrika.control(MD_MAX72XX::TEST, false);
    delay(200);
  }
  pokaziStevilo(vrstic, 3000);          // koliko vrstic je igralec počistil
}

// kos pade za eno vrstico; ko pristane, se doda h kupu
void padec() {
  if (prostoZa(kosX, kosY, sredX, sredY + 1)) {
    sredY++;
  } else {
    for (int i = 0; i < velikost; i++) {
      int y = sredY + kosY[i];
      if (y < 0) { konecIgre(); novaIgra(); return; }   // kup je prerasel ploščo
      kup[y][sredX + kosX[i]] = true;
    }
    pocistiVrstice();
    if (!novKos()) { konecIgre(); novaIgra(); return; }
  }
  naslednjiPadec = millis() + hitrost;
}

// ---------- daljinec ----------
void preberiDaljinec() {
  if (!IrReceiver.decode()) return;

  bool neznan = IrReceiver.decodedIRData.protocol == UNKNOWN;   // šum
  bool ponovitev = IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT;
  byte tipka = IrReceiver.decodedIRData.command;
  IrReceiver.resume();
  if (neznan) return;

  switch (tipka) {
    case T_LEVO:  case T_4: premakni(-1); break;
    case T_DESNO: case T_6: premakni( 1); break;
    case T_GOR:   case T_2: if (!ponovitev) zavrti(); break;   // držanje ne vrti naprej
    case T_DOL:   case T_8: padec(); break;
    // case T_SREDINA: break;   // play/pause: v tej igri ne naredi nič
  }
}

// ---------- program ----------
void setup() {
  Serial.begin(9600);
  matrika.begin();
  matrika.control(MD_MAX72XX::INTENSITY, 15);
  matrika.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);   // pošiljamo sami
  IrReceiver.begin(IR_PIN, ENABLE_LED_FEEDBACK);           // LED 13 utripne ob signalu
  randomSeed(analogRead(A5));   // vsakič drugačna igra
  pisk(880, 100);               // ob vklopu: preverimo, da piskač dela
  novaIgra();
}

void loop() {
  preberiDaljinec();
  if (millis() >= naslednjiPadec) padec();
  narisi();
}
