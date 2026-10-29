// Kockice: kosi padajo, polne vrstice izginejo (iz programa Kockice)
//   |<< levo, >>| desno, VOL+ zavrti, VOL- hitreje dol

namespace kockice {

// ---------- nastavitve ----------
const int ZAC_HITROST = 700;  // ms med padci na začetku
const int POSPESEK    = 40;   // za toliko ms hitreje po vsaki polni vrstici
const int NAJ_HITROST = 200;  // hitreje od tega ne gre
const int FAZ_KUP     = 3;    // zloženi kup sveti 1 od 3 ciklov, padajoči kos polno

// ---------- kosi ----------
// vsak kos ima do 3 kockice, zapisane kot zamik od središča (x, y)
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
int faza = 0;

// ena slika: kup je zatemnjen, padajoči kos sveti polno
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

void zvokVrstica() {          // kratek vesel pisk navzgor
  pisk(660, 70);
  pisk(880, 70);
  pisk(1320, 120);
}

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

void novaIgra() {
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 8; x++) kup[y][x] = false;
  vrstic = 0;
  hitrost = ZAC_HITROST;
  novKos();
  naslednjiPadec = millis() + hitrost;
  Serial.println("Nova igra!");
}

void konecIgre() {
  Serial.print("Konec igre. Vrstic: ");
  Serial.println(vrstic);
  zvokKonec();
  utripniCelo();
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

void tipka(byte t, bool ponovitev) {
  switch (t) {
    case T_LEVO:  premakni(-1); break;
    case T_DESNO: premakni( 1); break;
    case T_GOR:   if (!ponovitev) zavrti(); break;   // držanje ne vrti naprej
    case T_DOL:   padec(); break;
    // case T_SREDINA: break;   // play/pause: v tej igri ne naredi nič
  }
}

void zacni() { novaIgra(); }

void zanka() {
  if (millis() >= naslednjiPadec) padec();
  narisi();
}

}  // namespace kockice
