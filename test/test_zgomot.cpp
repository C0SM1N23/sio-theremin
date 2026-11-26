// Test bazait. In main.cpp, cand e liniste, DAC-ul sta la mijloc (2048 = 2,5 V). Tensiunea de pe A0
// e o fractiune din cei 5 V ai placii, deci zgomotul de pe 5 V (USB, modulul WiFi) ajunge si el,
// pe jumatate, la amplificator, chiar si la volum 0.
//
// Butonul encoderului (sau tastele 1-2 in Serial Monitor) alege:
//   1 MIJLOC FIX - ca in main.cpp: DAC-ul sta mereu in jurul lui 2048 (2,5 V)
//   2 MIJLOC=VOL - mijlocul coboara odata cu volumul; la volum 0 DAC-ul sta la 0 V
// Encoderul da volumul (La4, 440 Hz). Porneste la volum 0, in modul 1.
//
// Daca in modul 2, la volum 0, nu mai bazaie: zgomotul vine de pe 5 V prin DAC si se rezolva din cod.
// Daca bazaie la fel in ambele: zgomotul nu trece prin DAC (masa, alimentatorul de 12 V, laptopul).

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <FspTimer.h>
#include <math.h>

const int PIN_ENCODER_CLK = 8;
const int PIN_ENCODER_DT = 9;
const int PIN_ENCODER_BUTON = 7;

const int ADRESA_LCD = 0x27;

const float FRECVENTA_TEST_HZ = 440.0;

const int VOLUM_LA_PORNIRE = 0;
const int VOLUM_PAS = 5;
const int PASI_PE_CLIC = 2;
const unsigned long PAUZA_BUTON_MS = 50;

const float ESANTIOANE_PE_SECUNDA = 48000.0;
const double FAZA_O_PERIOADA = 4294967296.0;
const float PAS_PENTRU_1HZ = FAZA_O_PERIOADA / ESANTIOANE_PE_SECUNDA;
const int PRIORITATE_SUNET = 2;
const int MIJLOC_DAC = 2048;
const int VALOARE_MAXIMA_DAC = 4095;
// mijlocul se muta cu 1 la fiecare 8 esantioane: 0 -> 2048 in 0,34 s, la fel ca pornirea din main.cpp.
// Incet, ca schimbarea sa nu bufneasca prin condensatorul de la intrarea amplificatorului.
const int INCETINIRE_MIJLOC = 8;

const int NUMAR_MODURI = 2;
const char *NUME_MODURI[NUMAR_MODURI] = {"1 MIJLOC FIX", "2 MIJLOC=VOL"};

volatile int pasDorit = 0;
volatile int amplitudineDorita = 0;  // 0 .. 256
volatile bool mijlocUrmeazaVolumul = false;

uint32_t faza = 0;
int mijloc = 0;
int numaratorMijloc = 0;
int amplitudineAcum = 0;  // 0 .. 1024

int sinus[256];
FspTimer timerSunet;

int mod = 0;
int volum = VOLUM_LA_PORNIRE;

volatile int schimbariCLK = 0;
bool butonApasatInainte = false;
unsigned long ultimaSchimbareButon = 0;

LiquidCrystal_I2C lcd(ADRESA_LCD, 16, 2);

void esantionNou(timer_callback_args_t *) {
  int tinta = amplitudineDorita * 4;

  // la amplitudine maxima (1024) sinusul merge +-2047 in jurul mijlocului, deci mijlocul ajunge 2 * tinta
  int mijlocTinta = MIJLOC_DAC;
  if (mijlocUrmeazaVolumul) mijlocTinta = 2 * tinta;

  numaratorMijloc++;
  if (numaratorMijloc >= INCETINIRE_MIJLOC) {
    numaratorMijloc = 0;
    if (mijloc < mijlocTinta) mijloc++;
    else if (mijloc > mijlocTinta) mijloc--;
  }

  // cat timp mijlocul inca urca, sinusul nu are voie sa coboare sub 0
  if (tinta > mijloc / 2) tinta = mijloc / 2;
  if (amplitudineAcum < tinta) amplitudineAcum++;
  else if (amplitudineAcum > tinta) amplitudineAcum--;

  faza += pasDorit;

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

void afiseaza() {
  char rand1[17];
  char rand2[17];
  snprintf(rand1, sizeof(rand1), "%-16s", NUME_MODURI[mod]);
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
  mijlocUrmeazaVolumul = (mod == 1);
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

  pinMode(PIN_ENCODER_CLK, INPUT_PULLUP);
  pinMode(PIN_ENCODER_DT, INPUT_PULLUP);
  pinMode(PIN_ENCODER_BUTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_CLK), laSchimbareCLK, CHANGE);

  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Test bazait");

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

  Serial.println("Test bazait: buton encoder sau tastele 1-2 schimba modul, encoderul da volumul");
  amplitudineDorita = volum * 256 / 100;
  schimbaModul(0);
}

void loop() {
  citesteEncoderul();
  citesteButonul();
  citesteTastele();
}
