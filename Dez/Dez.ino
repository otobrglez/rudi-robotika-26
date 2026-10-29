// Lovljenje dežja: od zgoraj padajo kapljice, ti jih loviš s košaro na dnu.
//   |<< levo, >>| desno; tri zgrešene kapljice in je konec igre

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
const byte T_LEVO   = 0x44;  // |<<
const byte T_DESNO  = 0x43;  // >>|
const byte T_SREDINA = 0x40; // >|| (v tej igri je ne uporabljamo)

// ---------- nastavitve ----------
const int SIRINA_KOSARE = 2;
const int ZIVLJENJ    = 3;    // koliko kapljic lahko zgrešiš
const int ZAC_HITROST = 400;  // ms med padci na začetku
const int POSPESEK    = 6;    // za toliko ms hitreje po vsaki ujeti kapljici
const int NAJ_HITROST = 130;  // hitreje od tega ne gre
const int DEZ         = 35;   // verjetnost nove kapljice ob vsakem padcu, v %
const int NAJ_KAPLJIC = 12;   // več kapljic hkrati ne more biti

// ---------- stanje igre ----------
int kapX[NAJ_KAPLJIC], kapY[NAJ_KAPLJIC];
bool kapAktivna[NAJ_KAPLJIC];
int kosara;                // x skrajno leve pike košare, košara je v spodnji vrstici
int zivljenja;
int ujetih;                // točke
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
  if (x < 0 || x > 7 || y < 0 || y > 7) return;   // izven plošče se ne vidi
  buf[y] |= (B10000000 >> x);
}

void narisi() {
  byte buf[8] = { 0 };
  for (int i = 0; i < NAJ_KAPLJIC; i++)
    if (kapAktivna[i]) prizgi(buf, kapX[i], kapY[i]);
  for (int i = 0; i < SIRINA_KOSARE; i++) prizgi(buf, kosara + i, 7);
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

void zvokZgresil() {
  pisk(440, 100);
  pisk(220, 200);
}

void zvokKonec() {            // žalostna melodija navzdol
  pisk(523, 200);
  pisk(392, 200);
  pisk(330, 200);
  pisk(262, 500);
}

// ---------- igra ----------
void novaKapljica() {
  for (int i = 0; i < NAJ_KAPLJIC; i++) {
    if (kapAktivna[i]) continue;
    kapX[i] = random(8);
    kapY[i] = 0;
    kapAktivna[i] = true;
    return;
  }
}

void novaIgra() {
  for (int i = 0; i < NAJ_KAPLJIC; i++) kapAktivna[i] = false;
  kosara = (8 - SIRINA_KOSARE) / 2;   // košara na sredini
  zivljenja = ZIVLJENJ;
  ujetih = 0;
  hitrost = ZAC_HITROST;
  novaKapljica();
  naslednjiPadec = millis() + 800;    // malo časa, da se pripraviš
  Serial.println("Nova igra!");
}

void konecIgre() {
  Serial.print("Konec igre. Ujetih: ");
  Serial.println(ujetih);
  zvokKonec();
  for (int i = 0; i < 3; i++) {
    matrika.control(MD_MAX72XX::TEST, true);
    delay(200);
    matrika.control(MD_MAX72XX::TEST, false);
    delay(200);
  }
  pokaziStevilo(ujetih, 3000);
}

// vse kapljice padejo za eno vrstico
void padec() {
  for (int i = 0; i < NAJ_KAPLJIC; i++) {
    if (!kapAktivna[i]) continue;
    kapY[i]++;
    if (kapY[i] < 7) continue;

    // kapljica je na dnu: je tam košara?
    kapAktivna[i] = false;
    if (kapX[i] >= kosara && kapX[i] < kosara + SIRINA_KOSARE) {
      ujetih++;
      hitrost = max(NAJ_HITROST, hitrost - POSPESEK);
      tone(PISKAC, 1320, 40);          // kratek pisk, igra teče naprej
      Serial.print("Ujeta! ");
      Serial.println(ujetih);
    } else {
      zivljenja--;
      Serial.print("Zgresena! Zivljenja: ");
      Serial.println(zivljenja);
      narisi();
      zvokZgresil();
      if (zivljenja == 0) { konecIgre(); novaIgra(); return; }
    }
  }

  if (random(100) < DEZ) novaKapljica();
  naslednjiPadec = millis() + hitrost;
}

// ---------- daljinec ----------
// ko tipko držiš, se košara premika naprej
void preberiDaljinec() {
  if (!IrReceiver.decode()) return;

  bool neznan = IrReceiver.decodedIRData.protocol == UNKNOWN;   // šum
  byte tipka = IrReceiver.decodedIRData.command;
  IrReceiver.resume();
  if (neznan) return;

  switch (tipka) {
    case T_LEVO:  kosara = max(0, kosara - 1); break;
    case T_DESNO: kosara = min(8 - SIRINA_KOSARE, kosara + 1); break;
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
