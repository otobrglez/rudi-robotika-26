// Ognjemet: rakete letijo v nebo in eksplodirajo, s piskom in pokanjem
//   rakete se izstreljujejo same, z VOL+ izstreliš dodatno raketo

namespace ognjemet {

// ---------- nastavitve ----------
const int NAJ_RAKET     = 3;    // koliko raket je lahko hkrati v zraku
const int HITROST_LETA  = 80;   // ms, da raketa preleti eno vrstico
const int HITROST_POKA  = 90;   // ms med slikami eksplozije
const int MED_RAKETAMI_MIN = 600;    // ms do naslednje samodejne rakete
const int MED_RAKETAMI_MAX = 1600;
const int FAZ_SLED      = 3;    // sled rakete in ugašajoče iskre svetijo 1 od 3 ciklov

// ---------- stanje ----------
// stanje rakete: 0 = prosto, 1 = leti, 2 = eksplodira
int stanje[NAJ_RAKET];
int rakX[NAJ_RAKET], rakY[NAJ_RAKET];
int cilj[NAJ_RAKET];            // v kateri vrstici raketa poči
int korak[NAJ_RAKET];           // slika eksplozije: 1-3 se širi, 4-5 ugaša
unsigned long naslednji[NAJ_RAKET];
unsigned long naslednjaRaketa;
int faza = 0;

// 8 smeri iskric: vodoravno, navpično in poševno
const int iskraX[8] = { 1, -1, 0,  0, 1,  1, -1, -1 };
const int iskraY[8] = { 0,  0, 1, -1, 1, -1,  1, -1 };

void narisi() {
  byte buf[8] = { 0 };
  for (int i = 0; i < NAJ_RAKET; i++) {
    if (stanje[i] == 1) {                         // raketa in sled pod njo
      prizgi(buf, rakX[i], rakY[i]);
      if (faza == 0) prizgi(buf, rakX[i], rakY[i] + 1);
    }
    if (stanje[i] == 2) {                         // iskre okoli središča
      int r = min(korak[i], 3);
      bool ugasa = korak[i] > 3;
      if (ugasa && faza != 0) continue;           // ugašajoče iskre so temnejše
      for (int s = 0; s < 8; s++) {
        int d = (s < 4) ? r : max(1, r - 1);      // poševne iskre so malo krajše
        prizgi(buf, rakX[i] + iskraX[s] * d, rakY[i] + iskraY[s] * d);
      }
      if (korak[i] == 1) prizgi(buf, rakX[i], rakY[i]);   // blisk v središču
    }
  }
  poslji(buf);
  faza = (faza + 1) % FAZ_SLED;
}

void izstreli() {
  for (int i = 0; i < NAJ_RAKET; i++) {
    if (stanje[i] != 0) continue;
    stanje[i] = 1;
    rakX[i] = random(1, 7);
    rakY[i] = 7;
    cilj[i] = random(1, 4);                        // poči visoko na nebu
    naslednji[i] = millis() + HITROST_LETA;
    tone(PISKAC, 400, 20);
    return;
  }
}

void premakni(int i) {
  if (stanje[i] == 1) {                            // raketa leti gor in žvižga
    rakY[i]--;
    tone(PISKAC, 400 + (7 - rakY[i]) * 180, HITROST_LETA - 10);
    if (rakY[i] <= cilj[i]) {
      stanje[i] = 2;                               // BUM!
      korak[i] = 1;
      tone(PISKAC, 90, 70);
    }
    naslednji[i] = millis() + HITROST_LETA;
  } else if (stanje[i] == 2) {                     // eksplozija se širi in prasketa
    korak[i]++;
    if (korak[i] > 5) { stanje[i] = 0; return; }
    if (korak[i] <= 4) tone(PISKAC, random(900, 2500), 15);
    naslednji[i] = millis() + HITROST_POKA;
  }
}

void tipka(byte t, bool ponovitev) {
  switch (t) {
    case T_GOR: if (!ponovitev) izstreli(); break;   // dodatna raketa
    // case T_SREDINA: break;   // play/pause: tu ne naredi nič
  }
}

void zacni() {
  for (int i = 0; i < NAJ_RAKET; i++) stanje[i] = 0;
  izstreli();
  naslednjaRaketa = millis() + MED_RAKETAMI_MIN;
  Serial.println("Ognjemet!");
}

void zanka() {
  if (millis() >= naslednjaRaketa) {
    izstreli();
    naslednjaRaketa = millis() + random(MED_RAKETAMI_MIN, MED_RAKETAMI_MAX);
  }
  for (int i = 0; i < NAJ_RAKET; i++)
    if (stanje[i] != 0 && millis() >= naslednji[i]) premakni(i);
  narisi();
}

}  // namespace ognjemet
