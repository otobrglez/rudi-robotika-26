#include <MD_MAX72xx.h>
#include <SPI.h>

#define TIP   MD_MAX72XX::GENERIC_HW
#define DIN   7
#define CLK   6
#define CS    5

MD_MAX72XX matrika = MD_MAX72XX(TIP, DIN, CLK, CS, 1);

// ---------- nastavitve ----------
const int ST_PIK   = 10;   // koliko pikic je na začetku
const int HITROST  = 285;  // ms med koraki (manj = hitreje)
const int FAZ_KACE = 5;    // kača sveti 1 od 5 ciklov = 20 %

// ---------- stanje igre ----------
int kacaX[64], kacaY[64];  // [0] je glava
int dolzina;
bool pika[8][8];
int preostalo;
int stevecKorakov = 0;

int smerX[4] = { 1, -1, 0,  0 };
int smerY[4] = { 0,  0, 1, -1 };

// ---------- risanje ----------
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

void pripravi(byte buf[8], bool kaca, bool pike) {
  for (int i = 0; i < 8; i++) buf[i] = 0;
  if (kaca) {
    for (int i = 0; i < dolzina; i++) prizgi(buf, kacaX[i], kacaY[i]);
  }
  if (pike) {
    for (int y = 0; y < 8; y++)
      for (int x = 0; x < 8; x++)
        if (pika[y][x]) prizgi(buf, x, y);
  }
}

// prikazuje sliko ms milisekund, kača je zatemnjena
void prikazi(int ms, bool pokaziKaco, bool pokaziPike) {
  byte svetla[8], temna[8];
  pripravi(svetla, pokaziKaco, pokaziPike);   // kača + pike
  pripravi(temna, false, pokaziPike);         // samo pike

  unsigned long konec = millis() + ms;
  int faza = 0;
  while (millis() < konec) {
    if (faza == 0) poslji(svetla);
    else           poslji(temna);
    faza = (faza + 1) % FAZ_KACE;
  }
}

// ---------- pomožno ----------
bool jeTelo(int x, int y) {          // rep ne šteje, ker se bo umaknil
  for (int i = 0; i < dolzina - 1; i++)
    if (kacaX[i] == x && kacaY[i] == y) return true;
  return false;
}

bool naPlosci(int x, int y) {
  return x >= 0 && x < 8 && y >= 0 && y < 8;
}

int razdaljaDoPike(int x, int y) {
  int naj = 99;
  for (int py = 0; py < 8; py++)
    for (int px = 0; px < 8; px++)
      if (pika[py][px]) {
        int d = abs(px - x) + abs(py - y);
        if (d < naj) naj = d;
      }
  return naj;
}

// koliko prostih polj je dosegljivih z (sx, sy), da se kača ne zapre
int prostor(int sx, int sy) {
  bool obiskano[8][8] = { false };
  int vrstaX[64], vrstaY[64];
  int zac = 0, kon = 0, stevec = 0;
  vrstaX[kon] = sx; vrstaY[kon] = sy; kon++;
  obiskano[sy][sx] = true;
  while (zac < kon) {
    int x = vrstaX[zac], y = vrstaY[zac]; zac++;
    stevec++;
    for (int d = 0; d < 4; d++) {
      int nx = x + smerX[d], ny = y + smerY[d];
      if (naPlosci(nx, ny) && !obiskano[ny][nx] && !jeTelo(nx, ny)) {
        obiskano[ny][nx] = true;
        vrstaX[kon] = nx; vrstaY[kon] = ny; kon++;
      }
    }
  }
  return stevec;
}

// ---------- igra ----------
void novaIgra() {
  dolzina = 2;
  kacaX[0] = 3; kacaY[0] = 4;
  kacaX[1] = 2; kacaY[1] = 4;

  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 8; x++) pika[y][x] = false;

  preostalo = 0;
  while (preostalo < ST_PIK) {
    int x = random(8), y = random(8);
    bool naKaci = (x == kacaX[0] && y == kacaY[0]) || (x == kacaX[1] && y == kacaY[1]);
    if (!pika[y][x] && !naKaci) {
      pika[y][x] = true;
      preostalo++;
    }
  }
}

bool korak() {
  int najSmer = -1;
  long najTocke = -100000;

  for (int d = 0; d < 4; d++) {
    int nx = kacaX[0] + smerX[d];
    int ny = kacaY[0] + smerY[d];
    if (!naPlosci(nx, ny) || jeTelo(nx, ny)) continue;

    int prosto = prostor(nx, ny);
    long tocke = (prosto >= dolzina) ? 1000 : prosto * 10;  // varnost najprej
    tocke -= razdaljaDoPike(nx, ny) * 10;                    // nato bližina pikice
    tocke += random(5);                                      // malo naključja
    if (tocke > najTocke) { najTocke = tocke; najSmer = d; }
  }

  if (najSmer == -1) return false;   // ni izhoda

  int nx = kacaX[0] + smerX[najSmer];
  int ny = kacaY[0] + smerY[najSmer];

  if (pika[ny][nx]) {                // pojedla pikico, zraste
    pika[ny][nx] = false;
    preostalo--;
    dolzina++;
  }
  for (int i = dolzina - 1; i > 0; i--) {
    kacaX[i] = kacaX[i - 1];
    kacaY[i] = kacaY[i - 1];
  }
  kacaX[0] = nx; kacaY[0] = ny;
  return true;
}

void zmaga() {
  for (int i = 0; i < 3; i++) {
    matrika.control(MD_MAX72XX::TEST, true);
    delay(200);
    matrika.control(MD_MAX72XX::TEST, false);
    delay(200);
  }
}

void poraz() {
  for (int i = 0; i < 3; i++) {
    prikazi(250, true, false);
    prikazi(250, false, false);
  }
}

// ---------- program ----------
void setup() {
  matrika.begin();
  matrika.control(MD_MAX72XX::INTENSITY, 15);               // pikice 100 %
  matrika.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);     // pošiljamo sami
  randomSeed(analogRead(A5));   // vsakič drugačna igra
  novaIgra();
}

void loop() {
  if (!korak()) {
    poraz();
    novaIgra();
    return;
  }
  if (preostalo == 0) {
    prikazi(400, true, false);
    zmaga();
    novaIgra();
    return;
  }
  stevecKorakov++;
  bool pikeVidne = (stevecKorakov % 4 != 0);   // pike ugasnejo le vsak 4. korak
  prikazi(HITROST, true, pikeVidne);
}
