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
const byte T_GOR    = 0x46;  // VOL+  = prestava gor
const byte T_DOL    = 0x15;  // VOL-  = prestava dol
const byte T_LEVO   = 0x44;  // |<<   = levo
const byte T_DESNO  = 0x43;  // >>|   = desno
const byte T_SREDINA = 0x40; // >||   (v tej igri je ne uporabljamo)
const byte T_2 = 0x18, T_8 = 0x52, T_4 = 0x08, T_6 = 0x5A;  // tudi številke

// ---------- nastavitve ----------
const int SIRINA      = 4;    // koliko polj je široka cesta
const int ST_PRESTAV  = 5;
const int HITROST[ST_PRESTAV + 1] = { 0, 400, 320, 250, 190, 140 };  // ms na vrstico
const int OVIRE       = 15;   // verjetnost ovire v novi vrstici, v %
const int ZIVLJENJ    = 3;    // koliko trkov v ovire zdržiš
const int FAZ_ROBA    = 3;    // rob ceste sveti 1 od 3 ciklov, formula in ovire polno

// ---------- stanje igre ----------
int levo[8];        // kje se v vsaki vrstici začne cesta, [0] je zgoraj
int ovira[8];       // x ovire v vrstici ali -1
int formulaX;       // formula je v vrsticah 6 in 7
int prestava;
int zivljenja;
long tocke;
unsigned long naslednjiPomik;

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
  if (x < 0 || x > 7) return;         // rob izven plošče se ne vidi
  buf[y] |= (B10000000 >> x);
}

// ena slika: robovi ceste so zatemnjeni, formula in ovire svetijo polno
int faza = 0;
void narisi() {
  byte buf[8] = { 0 };
  for (int y = 0; y < 8; y++) {
    if (faza == 0) {
      prizgi(buf, levo[y] - 1, y);
      prizgi(buf, levo[y] + SIRINA, y);
    }
    if (ovira[y] >= 0) prizgi(buf, ovira[y], y);
  }
  prizgi(buf, formulaX, 6);
  prizgi(buf, formulaX, 7);
  poslji(buf);
  faza = (faza + 1) % FAZ_ROBA;
}

// ---------- pomožno ----------
bool naCesti(int x, int y) {
  return x >= levo[y] && x < levo[y] + SIRINA;
}

bool vOviri() {
  return ovira[6] == formulaX || ovira[7] == formulaX;
}

// če cesta zavije pod formulo, jo stena odrine nazaj na cesto
void odrini() {
  int skrajnoLevo  = max(levo[6], levo[7]);
  int skrajnoDesno = min(levo[6], levo[7]) + SIRINA - 1;
  formulaX = constrain(formulaX, skrajnoLevo, skrajnoDesno);
}

// nova vrstica na vrhu: cesta zavije za največ eno polje
void novaVrstica() {
  int l = levo[1] + random(-1, 2);
  levo[0] = constrain(l, 0, 8 - SIRINA);
  ovira[0] = -1;
  if (random(100) < OVIRE) ovira[0] = levo[0] + random(SIRINA);
}

// ---------- igra ----------
void novaIgra() {
  for (int y = 0; y < 8; y++) { levo[y] = 2; ovira[y] = -1; }   // ravna cesta brez ovir
  formulaX = 3;
  prestava = 1;
  zivljenja = ZIVLJENJ;
  tocke = 0;
  naslednjiPomik = millis() + HITROST[prestava];
  Serial.println("Start!");
}

void konecIgre() {
  Serial.print("Konec igre! Tocke: ");
  Serial.println(tocke);
  for (int i = 0; i < 3; i++) {
    matrika.control(MD_MAX72XX::TEST, true);
    delay(200);
    matrika.control(MD_MAX72XX::TEST, false);
    delay(200);
  }
}

// zadel si oviro: izgubiš eno življenje in prestavo 1, ovira izgine
void trk() {
  zivljenja--;
  Serial.print("Trk! Zivljenja: ");
  Serial.println(zivljenja);
  if (zivljenja == 0) { konecIgre(); novaIgra(); return; }

  ovira[6] = ovira[7] = -1;
  prestava = 1;
  for (int i = 0; i < 2; i++) {        // formula dvakrat utripne
    byte buf[8] = { 0 };
    poslji(buf);
    delay(150);
    prizgi(buf, formulaX, 6);
    prizgi(buf, formulaX, 7);
    poslji(buf);
    delay(150);
  }
  naslednjiPomik = millis() + HITROST[prestava];
}

// cesta se pomakne za eno vrstico dol (formula pelje naprej)
void pomik() {
  for (int y = 7; y > 0; y--) { levo[y] = levo[y - 1]; ovira[y] = ovira[y - 1]; }
  novaVrstica();
  tocke += prestava;                   // hitreje voziš, več točk dobiš
  odrini();
  naslednjiPomik = millis() + HITROST[prestava];
  if (vOviri()) trk();
}

void zavij(int dx) {
  int x = formulaX + dx;
  if (!naCesti(x, 6) || !naCesti(x, 7)) return;   // v steno ne moreš zaviti
  formulaX = x;
  if (vOviri()) trk();
}

void zamenjajPrestavo(int d) {
  prestava = constrain(prestava + d, 1, ST_PRESTAV);
  Serial.print("Prestava: ");
  Serial.println(prestava);
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
    case T_LEVO:  case T_4: zavij(-1); break;
    case T_DESNO: case T_6: zavij( 1); break;
    case T_GOR:   case T_2: if (!ponovitev) zamenjajPrestavo( 1); break;
    case T_DOL:   case T_8: if (!ponovitev) zamenjajPrestavo(-1); break;
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
  randomSeed(analogRead(A5));   // vsakič drugačna proga
  novaIgra();
}

void loop() {
  preberiDaljinec();
  if (millis() >= naslednjiPomik) pomik();
  narisi();
}
