// Strelec: od zgoraj se spuščajo kamni, spodaj s topom streljaš nanje
//   |<< levo, >>| desno, VOL+ strel; trije kamni na tleh in je konec igre

namespace strelec {

// ---------- nastavitve ----------
const int ZIVLJENJ     = 3;    // koliko kamnov lahko pade na tla
const int ZAC_HITROST  = 700;  // ms med padci kamnov na začetku
const int POSPESEK     = 10;   // za toliko ms hitreje po vsakem zadetku
const int NAJ_HITROST  = 250;  // hitreje od tega ne gre
const int HITROST_STRELA = 70; // ms, da strel preleti eno vrstico
const int MED_STRELI   = 200;  // najmanj ms med dvema streloma
const int KAMNI        = 40;   // verjetnost novega kamna ob vsakem padcu, v %
const int NAJ_KAMNOV   = 10;
const int NAJ_STRELOV  = 4;
const int FAZ_KAMNOV   = 2;    // kamni svetijo 1 od 2 ciklov, top in streli polno

// ---------- stanje igre ----------
int top;                   // x cevi topa; top je v vrsticah 6 in 7
int kamX[NAJ_KAMNOV], kamY[NAJ_KAMNOV];
bool kamAktiven[NAJ_KAMNOV];
int strX[NAJ_STRELOV], strY[NAJ_STRELOV];
bool strAktiven[NAJ_STRELOV];
int zivljenja;
int zadetkov;              // točke
int hitrost;
unsigned long naslednjiPadec, naslednjiLet, zadnjiStrel;
int faza = 0;

// ena slika: kamni so zatemnjeni, top in streli svetijo polno
void narisi() {
  byte buf[8] = { 0 };
  if (faza == 0)
    for (int i = 0; i < NAJ_KAMNOV; i++)
      if (kamAktiven[i]) prizgi(buf, kamX[i], kamY[i]);
  for (int i = 0; i < NAJ_STRELOV; i++)
    if (strAktiven[i]) prizgi(buf, strX[i], strY[i]);
  prizgi(buf, top, 6);                       // cev
  for (int x = top - 1; x <= top + 1; x++) prizgi(buf, x, 7);   // podstavek
  poslji(buf);
  faza = (faza + 1) % FAZ_KAMNOV;
}

// ali je na (x, y) kamen; vrne njegovo številko ali -1
int kamenNa(int x, int y) {
  for (int i = 0; i < NAJ_KAMNOV; i++)
    if (kamAktiven[i] && kamX[i] == x && kamY[i] == y) return i;
  return -1;
}

int strelNa(int x, int y) {
  for (int i = 0; i < NAJ_STRELOV; i++)
    if (strAktiven[i] && strX[i] == x && strY[i] == y) return i;
  return -1;
}

// strel in kamen se srečata: oba izgineta
void zadetek(int strel, int kamen) {
  strAktiven[strel] = false;
  kamAktiven[kamen] = false;
  zadetkov++;
  hitrost = max(NAJ_HITROST, hitrost - POSPESEK);
  tone(PISKAC, 660, 40);
  Serial.print("Zadetek! ");
  Serial.println(zadetkov);
}

void novKamen() {
  for (int i = 0; i < NAJ_KAMNOV; i++) {
    if (kamAktiven[i]) continue;
    kamX[i] = random(8);
    kamY[i] = 0;
    kamAktiven[i] = true;
    return;
  }
}

void ustreli() {
  if (millis() - zadnjiStrel < MED_STRELI) return;
  for (int i = 0; i < NAJ_STRELOV; i++) {
    if (strAktiven[i]) continue;
    strX[i] = top;
    strY[i] = 5;                             // tik nad cevjo
    strAktiven[i] = true;
    zadnjiStrel = millis();
    tone(PISKAC, 1760, 15);
    int k = kamenNa(strX[i], strY[i]);       // kamen tik nad cevjo
    if (k >= 0) zadetek(i, k);
    return;
  }
}

void novaIgra() {
  for (int i = 0; i < NAJ_KAMNOV; i++) kamAktiven[i] = false;
  for (int i = 0; i < NAJ_STRELOV; i++) strAktiven[i] = false;
  top = 3;
  zivljenja = ZIVLJENJ;
  zadetkov = 0;
  hitrost = ZAC_HITROST;
  novKamen();
  naslednjiPadec = millis() + 800;           // malo časa, da se pripraviš
  naslednjiLet = millis();
  Serial.println("Nova igra!");
}

void konecIgre() {
  Serial.print("Konec igre. Zadetkov: ");
  Serial.println(zadetkov);
  zvokKonec();
  utripniCelo();
  pokaziStevilo(zadetkov, 3000);
}

// streli letijo za eno vrstico gor
void let() {
  for (int i = 0; i < NAJ_STRELOV; i++) {
    if (!strAktiven[i]) continue;
    strY[i]--;
    if (strY[i] < 0) { strAktiven[i] = false; continue; }   // odletel s plošče
    int k = kamenNa(strX[i], strY[i]);
    if (k >= 0) zadetek(i, k);
  }
  naslednjiLet = millis() + HITROST_STRELA;
}

// kamni padejo za eno vrstico dol
void padec() {
  for (int i = 0; i < NAJ_KAMNOV; i++) {
    if (!kamAktiven[i]) continue;
    kamY[i]++;
    int s = strelNa(kamX[i], kamY[i]);
    if (s >= 0) { zadetek(s, i); continue; }

    if (kamY[i] >= 6) {                      // kamen je padel na tla
      kamAktiven[i] = false;
      zivljenja--;
      Serial.print("Kamen na tleh! Zivljenja: ");
      Serial.println(zivljenja);
      pisk(440, 100);
      pisk(220, 200);
      if (zivljenja == 0) { konecIgre(); novaIgra(); return; }
    }
  }

  if (random(100) < KAMNI) novKamen();
  naslednjiPadec = millis() + hitrost;
}

void tipka(byte t, bool ponovitev) {
  switch (t) {
    case T_LEVO:  top = max(1, top - 1); break;
    case T_DESNO: top = min(6, top + 1); break;
    case T_GOR:   ustreli(); break;          // držanje strelja naprej
    // case T_SREDINA: break;   // play/pause: v tej igri ne naredi nič
  }
}

void zacni() { novaIgra(); }

void zanka() {
  if (millis() >= naslednjiLet) let();
  if (millis() >= naslednjiPadec) padec();
  narisi();
}

}  // namespace strelec
