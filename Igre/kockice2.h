// Kockice 2: izboljšana različica Kockic (stara je v kockice.h)
//   |<< levo, >>| desno, VOL+ zavrti, VOL- hitreje dol,
//   ▼ (puščica dol) takoj na dno, >|| pavza
// Novo: senca kosa, takojšen spust, pavza, bonus za več vrstic hkrati,
// rekord, ki ostane shranjen tudi po izklopu (EEPROM).

namespace kockice2 {

// ---------- nastavitve ----------
const int ZAC_HITROST = 700;  // ms med padci na začetku
const int POSPESEK    = 40;   // za toliko ms hitreje po vsaki polni vrstici
const int NAJ_HITROST = 200;  // hitreje od tega ne gre
const int FAZ_KUP     = 3;    // zloženi kup sveti 1 od 3 ciklov, padajoči kos polno
const int FAZ_SENCA   = 6;    // senca sveti 1 od 6 ciklov: stalno, a temnejša od kupa
const int TOCKE[4]    = { 0, 1, 3, 6 };   // točke za 0, 1, 2 ali 3 vrstice hkrati

const byte T_SPUST = 0x07;    // ▼ = takoj na dno

// rekord v EEPROM: najprej oznaka, da vemo, da je zapis naš, nato število
const int EE_NASLOV = 10;
const byte EE_OZNAKA = 0x4B;

// ---------- kosi ----------
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
int vrstic;                  // koliko vrstic je igralec počistil
int tocke;
int rekord;
bool pavza = false;
int hitrost;
unsigned long naslednjiPadec;
int faza = 0;
int fazaSence = 0;

// ---------- rekord ----------
int preberiRekord() {
  if (EEPROM.read(EE_NASLOV) != EE_OZNAKA) return 0;   // še nikoli shranjen
  int r;
  EEPROM.get(EE_NASLOV + 1, r);
  return r;
}

void shraniRekord(int r) {
  EEPROM.update(EE_NASLOV, EE_OZNAKA);
  EEPROM.put(EE_NASLOV + 1, r);
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

// v kateri vrstici bi kos pristal, če bi padel naravnost dol
int vrsticaPristanka() {
  int y = sredY;
  while (prostoZa(kosX, kosY, sredX, y + 1)) y++;
  return y;
}

// ena slika: kup je zatemnjen, senca še bolj, padajoči kos sveti polno
void narisi() {
  byte buf[8] = { 0 };
  if (faza == 0)
    for (int y = 0; y < 8; y++)
      for (int x = 0; x < 8; x++)
        if (kup[y][x]) prizgi(buf, x, y);
  // senca: kam bo kos pristal (izklopljena; za vklop odstrani komentar)
  // if (!pavza && fazaSence == 0) {
  //   int py = vrsticaPristanka();
  //   for (int i = 0; i < velikost; i++) prizgi(buf, sredX + kosX[i], py + kosY[i]);
  // }
  if (!pavza || (millis() / 400) % 2 == 0)        // med pavzo kos počasi utripa
    for (int i = 0; i < velikost; i++) prizgi(buf, sredX + kosX[i], sredY + kosY[i]);
  poslji(buf);
  faza = (faza + 1) % FAZ_KUP;
  fazaSence = (fazaSence + 1) % FAZ_SENCA;
}

// ---------- zvok ----------
void zvokVrstice(int n) {     // več vrstic hkrati = daljša melodija
  int toni[4] = { 660, 880, 1100, 1320 };
  for (int i = 0; i < n + 1; i++) pisk(toni[i], 70);
  pisk(1760, 120);
}

void zvokRekord() {           // fanfara za nov rekord
  pisk(523, 120);
  pisk(659, 120);
  pisk(784, 120);
  pisk(1047, 400);
}

// ---------- igra ----------
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
  int n = 0;                                // koliko vrstic hkrati
  for (int y = 7; y >= 0; y--) {
    bool polna = true;
    for (int x = 0; x < 8; x++) if (!kup[y][x]) polna = false;
    if (!polna) continue;

    utripniVrstico(y);
    for (int yy = y; yy > 0; yy--)          // vse nad njo pade za eno dol
      for (int x = 0; x < 8; x++) kup[yy][x] = kup[yy - 1][x];
    for (int x = 0; x < 8; x++) kup[0][x] = false;

    n++;
    vrstic++;
    hitrost = max(NAJ_HITROST, hitrost - POSPESEK);
    y++;                                    // isto vrstico preveri še enkrat
  }
  if (n == 0) return;

  tocke += TOCKE[min(n, 3)];
  zvokVrstice(min(n, 3));
  Serial.print("Vrstic hkrati: ");
  Serial.print(n);
  Serial.print("  Tocke: ");
  Serial.println(tocke);
}

void novaIgra() {
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 8; x++) kup[y][x] = false;
  vrstic = 0;
  tocke = 0;
  pavza = false;
  hitrost = ZAC_HITROST;
  novKos();
  naslednjiPadec = millis() + hitrost;
  Serial.print("Nova igra! Rekord: ");
  Serial.println(rekord);
}

void konecIgre() {
  Serial.print("Konec igre. Tocke: ");
  Serial.print(tocke);
  Serial.print("  Vrstic: ");
  Serial.println(vrstic);
  zvokKonec();
  utripniCelo();
  pokaziStevilo(tocke, 2500);               // tvoje točke

  if (tocke > rekord) {                     // nov rekord: fanfara in shranimo
    rekord = tocke;
    shraniRekord(rekord);
    Serial.println("Nov rekord!");
    zvokRekord();
    pokaziStevilo(rekord, 1500);
  } else {                                  // stari rekord trikrat utripne
    for (int i = 0; i < 3; i++) {
      pokaziStevilo(rekord, 400);
      byte prazno[8] = { 0 };
      poslji(prazno);
      delay(200);
    }
  }
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
    tone(PISKAC, 150, 30);                  // "tok" ob pristanku
    pocistiVrstice();
    if (!novKos()) { konecIgre(); novaIgra(); return; }
  }
  naslednjiPadec = millis() + hitrost;
}

void spust() {                              // kos takoj na dno
  sredY = vrsticaPristanka();
  padec();
}

void tipka(byte t, bool ponovitev) {
  if (t == T_SREDINA && !ponovitev) {       // pavza vklop/izklop
    pavza = !pavza;
    Serial.println(pavza ? "Pavza" : "Naprej");
    tone(PISKAC, pavza ? 440 : 880, 60);
    naslednjiPadec = millis() + hitrost;
    return;
  }
  if (pavza) return;                        // med pavzo kosi ne poslušajo

  switch (t) {
    case T_LEVO:  premakni(-1); break;
    case T_DESNO: premakni( 1); break;
    case T_GOR:   if (!ponovitev) zavrti(); break;   // držanje ne vrti naprej
    case T_DOL:   padec(); break;
    case T_SPUST: if (!ponovitev) spust(); break;
  }
}

void zacni() {
  rekord = preberiRekord();
  novaIgra();
}

void zanka() {
  if (!pavza && millis() >= naslednjiPadec) padec();
  narisi();
}

}  // namespace kockice2
