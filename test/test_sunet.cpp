// Test sunet fara senzor: acelasi generator de sunet ca in main.cpp (timer 48 kHz -> DAC A0),
// dar nota e fixa (La4, 440 Hz). Asa vedem daca sunetul urat vine din montaj (amplificator,
// difuzor, alimentare) sau din lucrurile care merg in paralel cu sunetul.
//
// Butonul encoderului (sau tastele 1-4 in Serial Monitor) alege ce mai merge pe langa sunet:
//   1 SIMPLU   - doar sunetul
//   2 +AFISARE - sunet + LCD si Serial la fiecare 400 ms, ca in main.cpp
//   3 +SENZOR  - sunet + HC-SR04 citit la fiecare 30 ms, ca in main.cpp, dar distanta NU schimba nota
//   4 +TOATE   - sunet + LCD + senzor
// Encoderul da volumul. La volum 0 se aude doar zgomotul de fond.
// Ascultati fara sa atingeti encoderul: la rotire / apasare se scrie o data pe LCD si in Serial.
//
// Daca suna urat deja in modul 1: problema e in montaj sau volumul e prea mare (amplificatorul taie varfurile).
// Daca 1 e curat si 3 / 4 nu: deranjeaza senzorul (zgomot pe alimentare sau intarzieri).
// Daca 1 e curat si 2 nu: deranjeaza LCD-ul (I2C) sau Serial.
// Daca toate 4 sunt curate: sunetul urat din main.cpp vine din nota care tremura dupa senzor.

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <FspTimer.h>
#include <math.h>

const int PIN_TRIG = 4;
const int PIN_ECHO = 2;
const int PIN_ENCODER_CLK = 8;
const int PIN_ENCODER_DT = 9;
const int PIN_ENCODER_BUTON = 7;

const int ADRESA_LCD = 0x27;

const float FRECVENTA_TEST_HZ = 440.0;

const int VOLUM_LA_PORNIRE = 40;
const int VOLUM_PAS = 5;
const int PASI_PE_CLIC = 2;
const unsigned long PAUZA_BUTON_MS = 50;

const unsigned long ASTEPTARE_ECOU_US = 4000;
const double ECOU_US_PE_CM = 58.0;
const float NICIO_DISTANTA = -1;
const unsigned long PERIOADA_MASURARE_MS = 30;
const unsigned long PERIOADA_AFISARE_MS = 400;

// generatorul de sunet, la fel ca in main.cpp
const float ESANTIOANE_PE_SECUNDA = 48000.0;
const double FAZA_O_PERIOADA = 4294967296.0;
const float PAS_PENTRU_1HZ = FAZA_O_PERIOADA / ESANTIOANE_PE_SECUNDA;
const int ALUNECARE_CONT = 10;
const int PRIORITATE_SUNET = 2;
const int MIJLOC_DAC = 2048;
const int VALOARE_MAXIMA_DAC = 4095;
const int ESANTIOANE_PORNIRE = 16384;

const int NUMAR_MODURI = 4;
const char *NUME_MODURI[NUMAR_MODURI] = {"1 SIMPLU", "2 +AFISARE", "3 +SENZOR", "4 +TOATE"};

volatile int pasDorit = 0;
volatile int amplitudineDorita = 0;  // 0 .. 256
volatile int alunecare = ALUNECARE_CONT;

uint32_t faza = 0;
int pasAcum = 0;
int amplitudineAcum = 0;  // 0 .. 1024
int esantioaneDeLaPornire = 0;

int sinus[256];
FspTimer timerSunet;

int mod = 0;
int volum = VOLUM_LA_PORNIRE;
float ultimaDistanta = NICIO_DISTANTA;

volatile int schimbariCLK = 0;
bool butonApasatInainte = false;
unsigned long ultimaSchimbareButon = 0;

unsigned long ultimaMasurare = 0;
unsigned long ultimaAfisare = 0;

LiquidCrystal_I2C lcd(ADRESA_LCD, 16, 2);

// copiata din main.cpp, neschimbata
void esantionNou(timer_callback_args_t *) {
  if (esantioaneDeLaPornire < ESANTIOANE_PORNIRE) esantioaneDeLaPornire++;
  int mijloc = MIJLOC_DAC * esantioaneDeLaPornire / ESANTIOANE_PORNIRE;

  int tinta = amplitudineDorita * 4;
  if (amplitudineAcum < tinta) amplitudineAcum++;
  else if (amplitudineAcum > tinta) amplitudineAcum--;

  if (amplitudineAcum == 0) {
    pasAcum = pasDorit;
  } else {
    pasAcum += (pasDorit - pasAcum) >> alunecare;
  }

  faza += pasAcum;

  int punct = faza >> 24;
  int fractiune = (faza >> 16) & 0xFF;
  int a = sinus[punct];
  int b = sinus[(punct + 1) & 0xFF];
  int valoareSinus = a + (((b - a) * fractiune) >> 8);

  int valoare = mijloc + ((valoareSinus * amplitudineAcum) >> 10);
  if (valoare < 0) valoare = 0;
  if (valoare > VALOARE_MAXIMA_DAC) valoare = VALOARE_MAXIMA_DAC;
  R_DAC->DADR[0] = valoare;
}

bool pornesteSunetul() {
  uint8_t tipTimer;
  int8_t canal = FspTimer::get_available_timer(tipTimer);
  if (canal < 0) {
    FspTimer::force_use_of_pwm_reserved_timer();
    canal = FspTimer::get_available_timer(tipTimer, true);
  }
  if (canal < 0) return false;

  timerSunet.begin(TIMER_MODE_PERIODIC, tipTimer, canal, ESANTIOANE_PE_SECUNDA, 0.0f, esantionNou);
  timerSunet.setup_overflow_irq(PRIORITATE_SUNET);
  timerSunet.open();
  timerSunet.start();
  return true;
}

// copiata din main.cpp, neschimbata
float citesteDistanta() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  unsigned long durata = pulseInLong(PIN_ECHO, HIGH, ASTEPTARE_ECOU_US);
  if (durata == 0) return NICIO_DISTANTA;
  return durata / ECOU_US_PE_CM;
}

bool cuAfisare() {
  return mod == 1 || mod == 3;
}

bool cuSenzor() {
  return mod == 2 || mod == 3;
}

void afiseaza() {
  char rand1[17];
  char rand2[17];

  if (!cuSenzor()) snprintf(rand1, sizeof(rand1), "%-16s", NUME_MODURI[mod]);
  else if (ultimaDistanta == NICIO_DISTANTA) snprintf(rand1, sizeof(rand1), "%-11s --cm", NUME_MODURI[mod]);
  else snprintf(rand1, sizeof(rand1), "%-11s%3dcm", NUME_MODURI[mod], (int)lroundf(ultimaDistanta));

  snprintf(rand2, sizeof(rand2), "Vol %3d%%  %3d Hz", volum, (int)FRECVENTA_TEST_HZ);

  lcd.setCursor(0, 0);
  lcd.print(rand1);
  lcd.setCursor(0, 1);
  lcd.print(rand2);

  Serial.print(rand1);
  Serial.print(" | ");
  Serial.println(rand2);
}

void schimbaModul(int modNou) {
  mod = modNou;
  ultimaDistanta = NICIO_DISTANTA;
  afiseaza();
}

void laSchimbareCLK() {
  if (digitalRead(PIN_ENCODER_CLK) != digitalRead(PIN_ENCODER_DT)) schimbariCLK++;
  else schimbariCLK--;
}

void citesteEncoderul() {
  noInterrupts();
  int schimbari = schimbariCLK;
  schimbariCLK = schimbari % PASI_PE_CLIC;
  interrupts();

  int clicuri = schimbari / PASI_PE_CLIC;
  if (clicuri != 0) {
    volum = constrain(volum + clicuri * VOLUM_PAS, 0, 100);
    amplitudineDorita = volum * 256 / 100;
    afiseaza();
  }
}

void citesteButonul() {
  bool apasat = (digitalRead(PIN_ENCODER_BUTON) == LOW);

  if (apasat != butonApasatInainte && millis() - ultimaSchimbareButon > PAUZA_BUTON_MS) {
    ultimaSchimbareButon = millis();
    butonApasatInainte = apasat;
    if (apasat) schimbaModul((mod + 1) % NUMAR_MODURI);
  }
}

void citesteTastele() {
  if (!Serial.available()) return;
  char c = Serial.read();
  if (c >= '1' && c <= '0' + NUMAR_MODURI) schimbaModul(c - '1');
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_TRIG, OUTPUT);
  digitalWrite(PIN_TRIG, LOW);
  pinMode(PIN_ECHO, INPUT);

  pinMode(PIN_ENCODER_CLK, INPUT_PULLUP);
  pinMode(PIN_ENCODER_DT, INPUT_PULLUP);
  pinMode(PIN_ENCODER_BUTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_CLK), laSchimbareCLK, CHANGE);

  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Test sunet");

  for (int i = 0; i < 256; i++) {
    sinus[i] = lroundf(2047 * sinf(2 * PI * i / 256));
  }

  pasDorit = FRECVENTA_TEST_HZ * PAS_PENTRU_1HZ;

  analogWriteResolution(12);
  analogWrite(DAC, 0);
  if (!pornesteSunetul()) {
    Serial.println("Eroare: nu am gasit un timer liber pentru sunet");
  }
  delay(600);

  Serial.println("Test sunet: buton encoder sau tastele 1-4 schimba modul, encoderul da volumul");
  amplitudineDorita = volum * 256 / 100;
  schimbaModul(0);
}

void loop() {
  citesteEncoderul();
  citesteButonul();
  citesteTastele();

  if (cuSenzor() && millis() - ultimaMasurare >= PERIOADA_MASURARE_MS) {
    ultimaMasurare = millis();
    ultimaDistanta = citesteDistanta();  // doar pentru LCD, nota ramane la fel
  }

  if (cuAfisare() && millis() - ultimaAfisare >= PERIOADA_AFISARE_MS) {
    ultimaAfisare = millis();
    afiseaza();
  }
}
