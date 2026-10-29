// Lovljenje dežja: kapljice padajo, loviš jih s košaro na dnu (iz programa Dez)
//   |<< levo, >>| desno; tri zgrešene kapljice in je konec igre

namespace dez {

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

void narisi() {
  byte buf[8] = { 0 };
  for (int i = 0; i < NAJ_KAPLJIC; i++)
    if (kapAktivna[i]) prizgi(buf, kapX[i], kapY[i]);
  for (int i = 0; i < SIRINA_KOSARE; i++) prizgi(buf, kosara + i, 7);
  poslji(buf);
}

void zvokZgresil() {
  pisk(440, 100);
  pisk(220, 200);
}

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
  utripniCelo();
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

// ko tipko držiš, se košara premika naprej
void tipka(byte t, bool ponovitev) {
  switch (t) {
    case T_LEVO:  kosara = max(0, kosara - 1); break;
    case T_DESNO: kosara = min(8 - SIRINA_KOSARE, kosara + 1); break;
    // case T_SREDINA: break;   // play/pause: v tej igri ne naredi nič
  }
}

void zacni() { novaIgra(); }

void zanka() {
  if (millis() >= naslednjiPadec) padec();
  narisi();
}

}  // namespace dez
