#include <MD_MAX72xx.h>
#include <SPI.h>
#include <IRremote.hpp>

#define TIP   MD_MAX72XX::GENERIC_HW
#define DIN   7
#define CLK   6
#define CS    5
#define IR_PIN 2   // Y (signal) na IR sprejemniku

MD_MAX72XX matrika = MD_MAX72XX(TIP, DIN, CLK, CS, 1);

// ---------- tipke na daljincu ELEGOO ----------
const byte T_GOR    = 0x46;  // VOL+
const byte T_DOL    = 0x15;  // VOL-
const byte T_LEVO   = 0x44;  // |<<
const byte T_DESNO  = 0x43;  // >>|
const byte T_SREDINA = 0x40; // >||
const byte T_2 = 0x18, T_8 = 0x52, T_4 = 0x08, T_6 = 0x5A;  // tudi številke

// ---------- stanje ----------
int pikaX = 3, pikaY = 3;

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

void narisi() {
  byte buf[8] = { 0 };
  prizgi(buf, pikaX, pikaY);
  poslji(buf);
}

// ---------- premikanje ----------
void premakni(int dx, int dy) {
  pikaX = constrain(pikaX + dx, 0, 7);   // ne gre čez rob
  pikaY = constrain(pikaY + dy, 0, 7);
  narisi();
}

// ---------- program ----------
void setup() {
  Serial.begin(9600);
  matrika.begin();
  matrika.control(MD_MAX72XX::INTENSITY, 8);
  matrika.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);   // pošiljamo sami
  IrReceiver.begin(IR_PIN, ENABLE_LED_FEEDBACK);           // LED 13 utripne ob signalu
  narisi();
  Serial.println("Pripravljen. Pritisni tipko na daljincu.");
}

void loop() {
  if (!IrReceiver.decode()) return;

  byte tipka = IrReceiver.decodedIRData.command;
  bool ponovitev = IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT;
  IrReceiver.resume();

  Serial.print("Tipka 0x");
  Serial.print(tipka, HEX);
  Serial.println(ponovitev ? " (drzim)" : "");

  switch (tipka) {
    case T_GOR:   case T_2: premakni( 0, -1); break;
    case T_DOL:   case T_8: premakni( 0,  1); break;
    case T_LEVO:  case T_4: premakni(-1,  0); break;
    case T_DESNO: case T_6: premakni( 1,  0); break;
    case T_SREDINA:         pikaX = 3; pikaY = 3; narisi(); break;
  }
}
