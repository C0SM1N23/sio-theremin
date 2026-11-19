// Test sunet: A0 (DAC) -> divizor 10k/1k -> TDA2030A -> difuzor.
// Canta gama Do major, o glisare Do4 -> Do6 -> Do4, apoi La4 cu volumul in trepte.

#include <Arduino.h>
#include <FspTimer.h>
#include <math.h>

const float ESANTIOANE_PE_SECUNDA = 48000.0;
const float PAS_PENTRU_1HZ = 4294967296.0 / ESANTIOANE_PE_SECUNDA;

const float GAMA[]      = {261.63, 293.66, 329.63, 349.23, 392.00, 440.00, 493.88, 523.25};
const char *NUME_NOTE[] = {"C4", "D4", "E4", "F4", "G4", "A4", "B4", "C5"};

// Le scrie loop(), le citeste intreruperea de sunet
volatile int pasDorit = 0;
volatile int amplitudineDorita = 0;  // 0 .. 256, 256 = volum 100%

// Le foloseste doar intreruperea de sunet
uint32_t faza = 0;
int amplitudineAcum = 0;
int esantioaneDeLaPornire = 0;

int sinus[256];
FspTimer timerSunet;

void esantionNou(timer_callback_args_t *) {
  if (esantioaneDeLaPornire < 2048 * 8) esantioaneDeLaPornire++;
  int mijloc = esantioaneDeLaPornire / 8;

  if (amplitudineAcum < amplitudineDorita) amplitudineAcum++;
  else if (amplitudineAcum > amplitudineDorita) amplitudineAcum--;

  faza += pasDorit;

  int punct = faza >> 24;
  int departe = (faza >> 16) & 0xFF;
  int a = sinus[punct];
  int b = sinus[(punct + 1) & 0xFF];
  int valoareSinus = a + (((b - a) * departe) >> 8);

  int valoare = mijloc + ((valoareSinus * amplitudineAcum) >> 8);
  if (valoare < 0) valoare = 0;
  if (valoare > 4095) valoare = 4095;
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
  timerSunet.setup_overflow_irq(2);
  timerSunet.open();
  timerSunet.start();
  return true;
}

void canta(float frecventa, int volum) {
  pasDorit = frecventa * PAS_PENTRU_1HZ;
  amplitudineDorita = volum * 256 / 100;
}

void liniste() {
  amplitudineDorita = 0;
}

void setup() {
  Serial.begin(115200);
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
  Serial.println("Gama Do major, volum 30%");
  for (int i = 0; i < 8; i++) {
    Serial.print("  ");
    Serial.println(NUME_NOTE[i]);
    canta(GAMA[i], 30);
    delay(400);
  }
  liniste();
  delay(500);

  Serial.println("Glisare C4 -> C6 -> C4");
  for (float f = 261.63; f <= 1046.5; f *= 1.01) {
    canta(f, 30);
    delay(15);
  }
  for (float f = 1046.5; f >= 261.63; f /= 1.01) {
    canta(f, 30);
    delay(15);
  }
  liniste();
  delay(500);

  Serial.println("La4 (440 Hz), volum 10% -> 100%");
  for (int volum = 10; volum <= 100; volum += 30) {
    Serial.print("  volum ");
    Serial.print(volum);
    Serial.println("%");
    canta(440, volum);
    delay(700);
  }
  liniste();
  delay(1500);
}
