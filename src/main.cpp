// Theremin cu senzor HC-SR04 - Arduino UNO R4
// Mana deasupra senzorului da nota, encoderul da volumul, butonul encoderului schimba modul CONT / NOTE.

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <FspTimer.h>
#include <math.h>

const int PIN_TRIG = 4;
const int PIN_ECHO = 2;
const int PIN_ENCODER_CLK = 8;  // pe R4, D9 nu are intrerupere, D8 are
const int PIN_ENCODER_DT = 9;
const int PIN_ENCODER_BUTON = 7;

const int ADRESA_LCD = 0x27;

const float DISTANTA_MIN_CM = 5.0;
const float DISTANTA_MAX_CM = 50.0;
const float FRECVENTA_MIN_HZ = 261.63;  // Do4
const float NUMAR_OCTAVE = 2.0;
const double FRECVENTA_LA4 = 440.0;
const int NUMAR_MIDI_LA4 = 69;

const int VOLUM_LA_PORNIRE = 40;
const int VOLUM_PAS = 5;
const int PASI_PE_CLIC = 2;
const unsigned long PAUZA_BUTON_MS = 50;

const unsigned long ASTEPTARE_ECOU_US = 4000;
const double ECOU_US_PE_CM = 58.0;
const float NICIO_DISTANTA = -1;
const unsigned long PERIOADA_MASURARE_MS = 30;
const int RATARI_PANA_LA_LINISTE = 6;
const float SALT_MAXIM_CM = 8.0;
const float NETEZIRE_MIN = 0.1;
const float NETEZIRE_CRESTERE = 0.1;
const float PRAG_SCHIMBARE_NOTA = 0.7;

const unsigned long PERIOADA_AFISARE_MS = 400;

const float ESANTIOANE_PE_SECUNDA = 48000.0;
const double FAZA_O_PERIOADA = 4294967296.0;
const float PAS_PENTRU_1HZ = FAZA_O_PERIOADA / ESANTIOANE_PE_SECUNDA;
const int ALUNECARE_CONT = 10;
const int ALUNECARE_NOTE = 7;
const int PRIORITATE_SUNET = 2;
const int MIJLOC_DAC = 2048;
const int VALOARE_MAXIMA_DAC = 4095;
const int ESANTIOANE_PORNIRE = 16384;

const char *NUME_NOTE[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

volatile int pasDorit = 0;
volatile int amplitudineDorita = 0;  // 0 .. 256
volatile int alunecare = ALUNECARE_CONT;

uint32_t faza = 0;
int pasAcum = 0;
int amplitudineAcum = 0;  // 0 .. 1024
int esantioaneDeLaPornire = 0;

int sinus[256];
FspTimer timerSunet;

bool mana = false;
int ratari = 0;
float citireVeche = NICIO_DISTANTA;
float citireAnterioara = NICIO_DISTANTA;
float distantaNetezita = NICIO_DISTANTA;
float frecventa = FRECVENTA_MIN_HZ;
int notaCurenta = 0;
bool modNote = false;
int volum = VOLUM_LA_PORNIRE;

volatile int schimbariCLK = 0;
bool butonApasatInainte = false;
unsigned long ultimaSchimbareButon = 0;

unsigned long ultimaMasurare = 0;
unsigned long ultimaAfisare = 0;

LiquidCrystal_I2C lcd(ADRESA_LCD, 16, 2);

void esantionNou(timer_callback_args_t *) {
  // mijlocul semnalului urca incet la pornire, ca difuzorul sa nu pocneasca
  if (esantioaneDeLaPornire < ESANTIOANE_PORNIRE) esantioaneDeLaPornire++;
  int mijloc = MIJLOC_DAC * esantioaneDeLaPornire / ESANTIOANE_PORNIRE;

  int tinta = amplitudineDorita * 4;
  if (amplitudineAcum < tinta) amplitudineAcum++;
  else if (amplitudineAcum > tinta) amplitudineAcum--;

  if (amplitudineAcum == 0) {
    pasAcum = pasDorit;
  } else {
    pasAcum += (pasDorit - pasAcum) >> alunecare;  // >> 10 inseamna / 1024
  }

  faza += pasAcum;

  // primii 8 biti ai fazei aleg punctul din tabel, urmatorii 8 spun cat de aproape e punctul urmator
  int punct = faza >> 24;
  int fractiune = (faza >> 16) & 0xFF;
  int a = sinus[punct];
  int b = sinus[(punct + 1) & 0xFF];
  int valoareSinus = a + (((b - a) * fractiune) >> 8);

  int valoare = mijloc + ((valoareSinus * amplitudineAcum) >> 10);
  if (valoare < 0) valoare = 0;
  if (valoare > VALOARE_MAXIMA_DAC) valoare = VALOARE_MAXIMA_DAC;
  R_DAC->DADR[0] = valoare;  // analogWrite() e prea lent aici
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

float citesteDistanta() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  // pulseIn() masoara gresit din cauza intreruperii de sunet
  unsigned long durata = pulseInLong(PIN_ECHO, HIGH, ASTEPTARE_ECOU_US);
  if (durata == 0) return NICIO_DISTANTA;
  return durata / ECOU_US_PE_CM;
}

float mijlocDintreTrei(float a, float b, float c) {
  if ((a <= b && b <= c) || (c <= b && b <= a)) return b;
  if ((b <= a && a <= c) || (c <= a && a <= b)) return a;
  return c;
}

void uitaCitirile() {
  citireVeche = NICIO_DISTANTA;
  citireAnterioara = NICIO_DISTANTA;
  distantaNetezita = NICIO_DISTANTA;
}

void adaugaCitire(float d) {
  float citireBuna = d;
  if (citireVeche != NICIO_DISTANTA) citireBuna = mijlocDintreTrei(citireVeche, citireAnterioara, d);
  citireVeche = citireAnterioara;
  citireAnterioara = d;

  if (distantaNetezita == NICIO_DISTANTA) {
    distantaNetezita = citireBuna;
    return;
  }

  float miscare = constrain(citireBuna - distantaNetezita, -SALT_MAXIM_CM, SALT_MAXIM_CM);

  // miscarile mici (tremuratul senzorului) intra putin, cele mari intra toate
  float catIntra = NETEZIRE_MIN + NETEZIRE_CRESTERE * miscare * miscare;
  if (catIntra > 1) catIntra = 1;
  distantaNetezita += catIntra * miscare;
}

float frecventaPentruDistanta(float d) {
  d = constrain(d, DISTANTA_MIN_CM, DISTANTA_MAX_CM);
  float octaveUrcate = NUMAR_OCTAVE * (DISTANTA_MAX_CM - d) / (DISTANTA_MAX_CM - DISTANTA_MIN_CM);
  return FRECVENTA_MIN_HZ * powf(2.0, octaveUrcate);
}

float semitonuriFataDeLa4(float f) {
  return 12.0 * log2f(f / FRECVENTA_LA4);
}

void masoara() {
  float d = citesteDistanta();
  bool manaAbiaAparuta = false;

  if (d == NICIO_DISTANTA || d > DISTANTA_MAX_CM) {
    if (ratari < RATARI_PANA_LA_LINISTE) ratari++;
    if (ratari == RATARI_PANA_LA_LINISTE) {
      mana = false;
      uitaCitirile();
    }
  } else {
    if (!mana) manaAbiaAparuta = true;
    mana = true;
    ratari = 0;
    adaugaCitire(d);
  }

  if (mana) {
    frecventa = frecventaPentruDistanta(distantaNetezita);

    if (modNote) {
      float semitonuri = semitonuriFataDeLa4(frecventa);
      if (manaAbiaAparuta || fabsf(semitonuri - notaCurenta) > PRAG_SCHIMBARE_NOTA) {
        notaCurenta = lroundf(semitonuri);
      }
      frecventa = FRECVENTA_LA4 * powf(2.0, notaCurenta / 12.0);
    }

    pasDorit = frecventa * PAS_PENTRU_1HZ;
  }

  if (modNote) alunecare = ALUNECARE_NOTE;
  else alunecare = ALUNECARE_CONT;

  if (mana) amplitudineDorita = volum * 256 / 100;
  else amplitudineDorita = 0;
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
    if (mana) amplitudineDorita = volum * 256 / 100;
  }
}

void citesteButonul() {
  bool apasat = (digitalRead(PIN_ENCODER_BUTON) == LOW);

  if (apasat != butonApasatInainte && millis() - ultimaSchimbareButon > PAUZA_BUTON_MS) {
    ultimaSchimbareButon = millis();
    butonApasatInainte = apasat;
    if (apasat) modNote = !modNote;
  }
}

void afiseaza() {
  char rand1[17];
  char rand2[17];

  if (mana) {
    int numarNota = NUMAR_MIDI_LA4 + lroundf(semitonuriFataDeLa4(frecventa));
    char nota[5];
    snprintf(nota, sizeof(nota), "%s%d", NUME_NOTE[numarNota % 12], numarNota / 12 - 1);
    int zecimiHz = (int)(frecventa * 10 + 0.5);
    snprintf(rand1, sizeof(rand1), "%-4s %4d.%d Hz  ", nota, zecimiHz / 10, zecimiHz % 10);  // "C#5   554.4 Hz  "
  } else {
    snprintf(rand1, sizeof(rand1), "-- fara mana -- ");
  }

  if (modNote) snprintf(rand2, sizeof(rand2), "Vol %3d%%    NOTE", volum);
  else snprintf(rand2, sizeof(rand2), "Vol %3d%%    CONT", volum);

  lcd.setCursor(0, 0);
  lcd.print(rand1);
  lcd.setCursor(0, 1);
  lcd.print(rand2);

  Serial.print(rand1);
  Serial.print(" | ");
  Serial.print(rand2);
  if (mana) {
    Serial.print(" | ");
    Serial.print(distantaNetezita, 1);
    Serial.print(" cm");
  }
  Serial.println();
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
  lcd.print("Theremin SIO");

  for (int i = 0; i < 256; i++) {
    sinus[i] = lroundf(2047 * sinf(2 * PI * i / 256));
  }

  analogWriteResolution(12);
  analogWrite(DAC, 0);
  if (!pornesteSunetul()) {
    Serial.println("Eroare: nu am gasit un timer liber pentru sunet");
  }
  delay(600);
}

void loop() {
  citesteEncoderul();
  citesteButonul();

  if (millis() - ultimaMasurare >= PERIOADA_MASURARE_MS) {
    ultimaMasurare = millis();
    masoara();
  }

  if (millis() - ultimaAfisare >= PERIOADA_AFISARE_MS) {
    ultimaAfisare = millis();
    afiseaza();
  }
}
