// Kača: na tipke pobiraš pikice in rasteš (iz programa KacaDaljinec)
//   VOL+ gor, VOL- dol, |<< levo, >>| desno; vsak pritisk je en korak

namespace kaca {

// ---------- nastavitve ----------
const int FAZ_KACE = 5;    // telo kače sveti 1 od 5 ciklov = 20 %

// ---------- stanje igre ----------
int kacaX[64], kacaY[64];  // [0] je glava
int dolzina;
int pikaX, pikaY;
int smer;                  // 0 desno, 1 levo, 2 dol, 3 gor (zadnji premik)
int faza = 0;

const int smerX[4] = { 1, -1, 0,  0 };
const int smerY[4] = { 0,  0, 1, -1 };
const int nasprotna[4] = { 1, 0, 3, 2 };

// ena slika: telo kače je zatemnjeno, glava in pikica svetita polno
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
  utripniCelo();
}

void tipka(byte t, bool ponovitev) {
  int s = -1;
  switch (t) {
    case T_DESNO: s = 0; break;
    case T_LEVO:  s = 1; break;
    case T_DOL:   s = 2; break;
    case T_GOR:   s = 3; break;
    // case T_SREDINA: break;   // play/pause: v tej igri ne naredi nič
  }
  if (s == -1) return;
  if (dolzina > 1 && s == nasprotna[smer]) return;   // obrat nazaj vase ni dovoljen

  if (!korak(s)) {
    poraz();
    novaIgra();
  } else if (dolzina == 64) {
    zmaga();
    novaIgra();
  }
}

void zacni() { novaIgra(); }

void zanka() { narisi(); }   // kača se premika samo ob pritisku tipke

}  // namespace kaca
