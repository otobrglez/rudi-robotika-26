#include <MD_MAX72xx.h>
#include <SPI.h>
#define IR_USE_AVR_TIMER5   // daljinec na časovniku 5, ker tone() za piskač rabi časovnik 2
#include <IRremote.hpp>

#define TIP   MD_MAX72XX::GENERIC_HW
#define DIN   7
#define CLK   6
#define CS    5
#define IR_PIN 2      // Y (signal) na IR sprejemniku
#define PISKAC 8      // + nožica pasivnega piskača

MD_MAX72XX matrika = MD_MAX72XX(TIP, DIN, CLK, CS, 1);

// ---------- tipke na daljincu ELEGOO ----------
const byte T_LEVO   = 0x44;  // |<<   = lopar levo
const byte T_DESNO  = 0x43;  // >>|   = lopar desno
const byte T_SREDINA = 0x40; // >||   (v tej igri je ne uporabljamo)
const byte T_4 = 0x08, T_6 = 0x5A;  // tudi številke

// ---------- nastavitve ----------
const int SIRINA_LOPARJA = 3;
const int ZIVLJENJ    = 3;
const int ZAC_HITROST = 260;  // ms med koraki žogice na začetku
const int POSPESEK    = 8;    // za toliko ms hitreje po vsakem odboju od loparja
const int NAJ_HITROST = 90;   // hitreje od tega ne gre
const int NOVA_OVIRA  = 2;    // vsakič po toliko odbojih od loparja se pojavi ovira
const int NAJ_OVIR    = 12;   // več ovir hkrati ne more biti
const int FAZ_OVIR    = 3;    // ovire svetijo 1 od 3 ciklov, žogica in lopar polno

// ---------- stanje igre ----------
int zogaX, zogaY;          // [0, 0] je zgoraj levo
int smerX, smerY;          // -1, 0 ali 1
int lopar;                 // x skrajno leve pike loparja, lopar je v spodnji vrstici
bool ovira[8][8];          // ovire [y][x], pojavljajo se v vrsticah 1 do 4
int stOvir;
int zivljenja;
int odbojev;               // kolikokrat si odbil žogico z loparjem
int tocke;                 // odboji + zadete ovire
int hitrost;
unsigned long naslednjiKorak;

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
  buf[y] |= (B10000000 >> x);
}

int faza = 0;
void narisi() {
  byte buf[8] = { 0 };
  if (faza == 0)
    for (int y = 0; y < 8; y++)
      for (int x = 0; x < 8; x++)
        if (ovira[y][x]) prizgi(buf, x, y);
  faza = (faza + 1) % FAZ_OVIR;
  for (int i = 0; i < SIRINA_LOPARJA; i++) prizgi(buf, lopar + i, 7);
  prizgi(buf, zogaX, zogaY);
  poslji(buf);
}

// ---------- številke (kot pri Kockicah) ----------
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

// ---------- zvok ----------
// zaigra ton in počaka, da se konča
void pisk(int hz, int ms) {
  tone(PISKAC, hz, ms);
  delay(ms + 20);
}

// kratek ton, igra med tem teče naprej
void klik(int hz) {
  tone(PISKAC, hz, 30);
}

void zvokZgresil() {
  pisk(440, 120);
  pisk(220, 250);
}

void zvokKonec() {            // žalostna melodija navzdol
  pisk(523, 200);
  pisk(392, 200);
  pisk(330, 200);
  pisk(262, 500);
}

// ---------- daljinec ----------
// ko tipko držiš, se lopar premika naprej
void preberiDaljinec() {
  if (!IrReceiver.decode()) return;

  bool neznan = IrReceiver.decodedIRData.protocol == UNKNOWN;   // šum
  byte tipka = IrReceiver.decodedIRData.command;
  IrReceiver.resume();
  if (neznan) return;

  switch (tipka) {
    case T_LEVO:  case T_4: lopar = max(0, lopar - 1); break;
    case T_DESNO: case T_6: lopar = min(8 - SIRINA_LOPARJA, lopar + 1); break;
    // case T_SREDINA: break;   // play/pause: v tej igri ne naredi nič
  }
}

// ---------- igra ----------
void novaZoga() {
  zogaX = random(2, 6);
  zogaY = 0;                           // v vrstici 0 ni nikoli ovir
  smerX = random(2) ? 1 : -1;
  smerY = 1;
  naslednjiKorak = millis() + 800;     // malo časa, da se pripraviš
}

// nova ovira na naključnem prostem mestu v vrsticah 1 do 4
void novaOvira() {
  if (stOvir >= NAJ_OVIR) return;
  int x, y;
  do {
    x = random(8);
    y = random(1, 5);
  } while (ovira[y][x] || (x == zogaX && y == zogaY));
  ovira[y][x] = true;
  stOvir++;
  klik(1320);
}

void odstraniOviro(int x, int y) {
  ovira[y][x] = false;
  stOvir--;
  tocke++;
  Serial.print("Ovira! Tocke: ");
  Serial.println(tocke);
}

void novaIgra() {
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 8; x++) ovira[y][x] = false;
  stOvir = 0;
  zivljenja = ZIVLJENJ;
  odbojev = 0;
  tocke = 0;
  lopar = (8 - SIRINA_LOPARJA) / 2;    // lopar na sredini
  hitrost = ZAC_HITROST;
  novaZoga();
  Serial.println("Nova igra!");
}

void konecIgre() {
  Serial.print("Konec igre. Tocke: ");
  Serial.println(tocke);
  zvokKonec();
  for (int i = 0; i < 3; i++) {
    matrika.control(MD_MAX72XX::TEST, true);
    delay(200);
    matrika.control(MD_MAX72XX::TEST, false);
    delay(200);
  }
  pokaziStevilo(tocke, 3000);
}

void korak() {
  // stene levo in desno
  int nx = zogaX + smerX;
  if (nx < 0 || nx > 7) {
    smerX = -smerX;
    nx = zogaX + smerX;
    klik(440);
  }

  // strop
  int ny = zogaY + smerY;
  if (ny < 0) {
    smerY = 1;
    ny = zogaY + smerY;
    klik(440);
  }

  // ovire: žogica se odbije in ovira izgine
  bool vodoravno = smerX != 0 && ovira[zogaY][nx];   // ovira ob strani
  bool navpicno  = ovira[ny][zogaX];                  // ovira zgoraj ali spodaj
  if (vodoravno || navpicno || ovira[ny][nx]) {
    if (vodoravno) { odstraniOviro(nx, zogaY); smerX = -smerX; }
    if (navpicno)  { odstraniOviro(zogaX, ny); smerY = -smerY; }
    if (!vodoravno && !navpicno) {                    // zadela v kot
      odstraniOviro(nx, ny);
      smerX = -smerX;
      smerY = -smerY;
    }
    klik(660);
    naslednjiKorak = millis() + hitrost;
    return;                                           // ta korak žogica obstane
  }

  // spodnja vrstica: je tam lopar?
  if (ny == 7) {
    int kje = nx - lopar;                    // 0 levi rob, 1 sredina, 2 desni rob
    if (kje >= 0 && kje < SIRINA_LOPARJA) {
      smerY = -1;
      smerX = kje - 1;                       // rob loparja pošlje žogico postrani
      if (smerX == 0 && random(4) == 0)      // včasih sredina malo zanese
        smerX = random(2) ? 1 : -1;
      odbojev++;
      tocke++;
      hitrost = max(NAJ_HITROST, hitrost - POSPESEK);
      klik(880);
      Serial.print("Odboj! Tocke: ");
      Serial.println(tocke);
      if (odbojev % NOVA_OVIRA == 0) novaOvira();
      naslednjiKorak = millis() + hitrost;
      return;                                // žogica ostane nad loparjem
    }
  }

  zogaX = nx;
  zogaY = ny;

  if (zogaY == 7) {                          // zgrešil si
    narisi();
    zivljenja--;
    Serial.print("Zgresil! Zivljenja: ");
    Serial.println(zivljenja);
    zvokZgresil();
    if (zivljenja == 0) { konecIgre(); novaIgra(); }
    else novaZoga();
    return;
  }
  naslednjiKorak = millis() + hitrost;
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
  if (millis() >= naslednjiKorak) korak();
  narisi();
}
