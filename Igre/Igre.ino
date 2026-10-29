// Igre v enem programu. Igro izbereš s tipko na daljincu:
//   3 = Kockice 2, 4 = Ognjemet, 5 = Strelec, 6 = Lovljenje dežja, 7 = Kockice, 8 = Formula, 9 = Kača
// Ob vklopu matrika izmenično kaže 3, 4, 5, 6, 7, 8 in 9, dokler ne izbereš igre.
// Vsaka igra je v svoji datoteki (kockice2.h, ognjemet.h, strelec.h, dez.h, kockice.h, formula.h, kaca.h).

#include <MD_MAX72xx.h>
#include <SPI.h>
#include <EEPROM.h>   // za rekord v Kockicah 2
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
const byte T_GOR     = 0x46;  // VOL+
const byte T_DOL     = 0x15;  // VOL-
const byte T_LEVO    = 0x44;  // |<<
const byte T_DESNO   = 0x43;  // >>|
const byte T_SREDINA = 0x40;  // >|| (v igrah je ne uporabljamo)
const byte T_3 = 0x5E;        // izberi Kockice 2
const byte T_4 = 0x08;        // izberi Ognjemet
const byte T_5 = 0x1C;        // izberi Strelca
const byte T_6 = 0x5A;        // izberi Lovljenje dežja
const byte T_7 = 0x42;        // izberi Kockice
const byte T_8 = 0x52;        // izberi Formulo
const byte T_9 = 0x4A;        // izberi Kačo

// ---------- risanje (skupno vsem igram) ----------
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

void utripniCelo() {          // vsa matrika trikrat utripne
  for (int i = 0; i < 3; i++) {
    matrika.control(MD_MAX72XX::TEST, true);
    delay(200);
    matrika.control(MD_MAX72XX::TEST, false);
    delay(200);
  }
}

// ---------- številke 0-9, vsaka je velika 3 x 5 pik ----------
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
// zaigra ton s frekvenco hz (Hz) dolžine ms in počaka, da se konča
void pisk(int hz, int ms) {
  tone(PISKAC, hz, ms);
  delay(ms + 20);
}

void zvokKonec() {            // žalostna melodija navzdol
  pisk(523, 200);
  pisk(392, 200);
  pisk(330, 200);
  pisk(262, 500);
}

// ---------- igre ----------
#include "kockice2.h"
#include "ognjemet.h"
#include "strelec.h"
#include "dez.h"
#include "kockice.h"
#include "formula.h"
#include "kaca.h"

// ---------- meni ----------
int igra = 0;                 // 0 = meni, 3 = Kockice 2, 4 = Ognjemet, 5 = Strelec, 6 = Dež, 7 = Kockice, 8 = Formula, 9 = Kača

void izberiIgro(int i) {
  igra = i;
  pisk(1320, 60);
  pokaziStevilo(i, 600);      // kratko pokaže, katera igra se začne
  if (igra == 3) { Serial.println("Igra: Kockice 2"); kockice2::zacni(); }
  if (igra == 4) { Serial.println("Igra: Ognjemet"); ognjemet::zacni(); }
  if (igra == 5) { Serial.println("Igra: Strelec"); strelec::zacni(); }
  if (igra == 6) { Serial.println("Igra: Dez");     dez::zacni(); }
  if (igra == 7) { Serial.println("Igra: Kockice"); kockice::zacni(); }
  if (igra == 8) { Serial.println("Igra: Formula"); formula::zacni(); }
  if (igra == 9) { Serial.println("Igra: Kaca");    kaca::zacni(); }
}

void meni() {                 // izmenično kaže 3, 4, 5, 6, 7, 8 in 9
  byte buf[8] = { 0 };
  narisiStevko(buf, 3 + (millis() / 700) % 7, 2);
  poslji(buf);
}

// ---------- daljinec ----------
void preberiDaljinec() {
  if (!IrReceiver.decode()) return;

  bool neznan = IrReceiver.decodedIRData.protocol == UNKNOWN;   // šum
  bool ponovitev = IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT;
  byte tipka = IrReceiver.decodedIRData.command;
  IrReceiver.resume();
  if (neznan) return;

  if (!ponovitev) {                    // izpis kode, da vidimo, katera tipka je prišla
    Serial.print("Tipka 0x");
    Serial.println(tipka, HEX);
  }

  if (!ponovitev) {                    // izbira igre (držanje ne šteje)
    if (tipka == T_3) { izberiIgro(3); return; }
    if (tipka == T_4) { izberiIgro(4); return; }
    if (tipka == T_5) { izberiIgro(5); return; }
    if (tipka == T_6) { izberiIgro(6); return; }
    if (tipka == T_7) { izberiIgro(7); return; }
    if (tipka == T_8) { izberiIgro(8); return; }
    if (tipka == T_9) { izberiIgro(9); return; }
  }

  // ostale tipke dobi igra, ki teče
  if (igra == 3) kockice2::tipka(tipka, ponovitev);
  if (igra == 4) ognjemet::tipka(tipka, ponovitev);
  if (igra == 5) strelec::tipka(tipka, ponovitev);
  if (igra == 6) dez::tipka(tipka, ponovitev);
  if (igra == 7) kockice::tipka(tipka, ponovitev);
  if (igra == 8) formula::tipka(tipka, ponovitev);
  if (igra == 9) kaca::tipka(tipka, ponovitev);
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
  Serial.println("Izberi igro: 3 = Kockice 2, 4 = Ognjemet, 5 = Strelec, 6 = Dez, 7 = Kockice, 8 = Formula, 9 = Kaca");
}

void loop() {
  preberiDaljinec();
  if (igra == 0) meni();
  if (igra == 3) kockice2::zanka();
  if (igra == 4) ognjemet::zanka();
  if (igra == 5) strelec::zanka();
  if (igra == 6) dez::zanka();
  if (igra == 7) kockice::zanka();
  if (igra == 8) formula::zanka();
  if (igra == 9) kaca::zanka();
}
