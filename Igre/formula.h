// Formula: voziš po vijugasti cesti mimo ovir (iz programa Formula)
//   |<< levo, >>| desno, VOL+ prestava gor, VOL- prestava dol

namespace formula {

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
int faza = 0;

// ena slika: robovi ceste so zatemnjeni, formula in ovire svetijo polno
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
  utripniCelo();
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

void tipka(byte t, bool ponovitev) {
  switch (t) {
    case T_LEVO:  zavij(-1); break;
    case T_DESNO: zavij( 1); break;
    case T_GOR:   if (!ponovitev) zamenjajPrestavo( 1); break;
    case T_DOL:   if (!ponovitev) zamenjajPrestavo(-1); break;
    // case T_SREDINA: break;   // play/pause: v tej igri ne naredi nič
  }
}

void zacni() { novaIgra(); }

void zanka() {
  if (millis() >= naslednjiPomik) pomik();
  narisi();
}

}  // namespace formula
