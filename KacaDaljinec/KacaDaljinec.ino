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
const byte T_SREDINA = 0x40; // >|| (v tej igri je ne uporabljamo)
const byte T_2 = 0x18, T_8 = 0x52, T_4 = 0x08, T_6 = 0x5A;  // tudi številke

// ---------- nastavitve ----------
const int FAZ_KACE    = 5;    // telo kače sveti 1 od 5 ciklov = 20 %

// ---------- stanje igre ----------
int kacaX[64], kacaY[64];  // [0] je glava
int dolzina;
int pikaX, pikaY;
int smer;                  // 0 desno, 1 levo, 2 dol, 3 gor (zadnji premik)

int smerX[4] = { 1, -1, 0,  0 };
int smerY[4] = { 0,  0, 1, -1 };
int nasprotna[4] = { 1, 0, 3, 2 };

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

// ena slika: telo kače je zatemnjeno, glava in pikica svetita polno
int faza = 0;
void narisi() {
  byte buf[8] = { 0 };
  if (faza == 0)
    for (int i = 1; i < dolzina; i++) prizgi(buf, kacaX[i], kacaY[i]);
  prizgi(buf, kacaX[0], kacaY[0]);
  prizgi(buf, pikaX, pikaY);
  poslji(buf);
  faza = (faza + 1) % FAZ_KACE;
}

// prikazuje celo kačo (svetla = true) ali prazno ploščo ms milisekund
void utripni(int ms, bool svetla) {
  byte buf[8] = { 0 };
  if (svetla)
    for (int i = 0; i < dolzina; i++) prizgi(buf, kacaX[i], kacaY[i]);
  poslji(buf);
  delay(ms);
}

// ---------- pomožno ----------
bool jeTelo(int x, int y) {          // rep ne šteje, ker se bo umaknil
  for (int i = 0; i < dolzina - 1; i++)
    if (kacaX[i] == x && kacaY[i] == y) return true;
  return false;
}

bool naKaci(int x, int y) {
  for (int i = 0; i < dolzina; i++)
    if (kacaX[i] == x && kacaY[i] == y) return true;
  return false;
}

void novaPika() {
  do {
    pikaX = random(8);
    pikaY = random(8);
  } while (naKaci(pikaX, pikaY));
}

// ---------- igra ----------
void novaIgra() {
  dolzina = 2;
  kacaX[0] = 3; kacaY[0] = 4;
  kacaX[1] = 2; kacaY[1] = 4;
  smer = 0;
  novaPika();
  Serial.println("Nova igra!");
}

// en korak v smer s; vrne false, če se je kača zaletela vase
bool korak(int s) {
  smer = s;
  int nx = (kacaX[0] + smerX[smer] + 8) % 8;   // čez rob pride na drugo stran
  int ny = (kacaY[0] + smerY[smer] + 8) % 8;

  if (jeTelo(nx, ny)) return false;

  bool pojedla = (nx == pikaX && ny == pikaY);
  if (pojedla) dolzina++;                       // zraste za eno polje

  for (int i = dolzina - 1; i > 0; i--) {
    kacaX[i] = kacaX[i - 1];
    kacaY[i] = kacaY[i - 1];
  }
  kacaX[0] = nx; kacaY[0] = ny;

  if (pojedla) {
    Serial.print("Pikica! Dolzina: ");
    Serial.println(dolzina);
    if (dolzina < 64) novaPika();
  }
  return true;
}

void poraz() {
  Serial.print("Konec igre. Dolzina: ");
  Serial.println(dolzina);
  for (int i = 0; i < 3; i++) {
    utripni(250, true);
    utripni(250, false);
  }
}

void zmaga() {
  Serial.println("Zmaga! Cela plosca je polna.");
  for (int i = 0; i < 3; i++) {
    matrika.control(MD_MAX72XX::TEST, true);
    delay(200);
    matrika.control(MD_MAX72XX::TEST, false);
    delay(200);
  }
}

// ---------- daljinec ----------
// vrne smer pritisnjene tipke ali -1, če ni nič pritisnjeno
int preberiDaljinec() {
  if (!IrReceiver.decode()) return -1;

  bool neznan = IrReceiver.decodedIRData.protocol == UNKNOWN;   // šum
  byte tipka = IrReceiver.decodedIRData.command;
  IrReceiver.resume();
  if (neznan) return -1;

  int s = -1;
  switch (tipka) {
    case T_DESNO: case T_6: s = 0; break;
    case T_LEVO:  case T_4: s = 1; break;
    case T_DOL:   case T_8: s = 2; break;
    case T_GOR:   case T_2: s = 3; break;
    // case T_SREDINA: break;   // play/pause: v tej igri ne naredi nič
  }
  // obrat nazaj vase ni dovoljen
  if (s != -1 && dolzina > 1 && s == nasprotna[smer]) return -1;
  return s;
}

// ---------- program ----------
void setup() {
  Serial.begin(9600);
  matrika.begin();
  matrika.control(MD_MAX72XX::INTENSITY, 15);
  matrika.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);   // pošiljamo sami
  IrReceiver.begin(IR_PIN, ENABLE_LED_FEEDBACK);           // LED 13 utripne ob signalu
  randomSeed(analogRead(A5));   // vsakič drugačna igra
  novaIgra();
}

void loop() {
  int s = preberiDaljinec();

  if (s != -1) {                // kača se premakne samo ob pritisku tipke
    if (!korak(s)) {
      poraz();
      novaIgra();
      return;
    }
    if (dolzina == 64) {
      zmaga();
      novaIgra();
      return;
    }
  }

  narisi();
}
