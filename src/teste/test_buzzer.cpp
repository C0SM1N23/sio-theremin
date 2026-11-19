// Test buzzer pasiv pe D11 (prin 1k). Volumul se face din factorul de umplere: 100% = umplere 50%.

#include <Arduino.h>
#include <pwm.h>

const int PIN_BUZZER = 11;
const float FRECVENTA_REFERINTA = 200.0;  // cea mai joasa frecventa folosita

const float GAMA[]      = {261.63, 293.66, 329.63, 349.23, 392.00, 440.00, 493.88, 523.25};
const char *NUME_NOTE[] = {"C4", "D4", "E4", "F4", "G4", "A4", "B4", "C5"};

PwmOut buzzer(PIN_BUZZER);
float pasiTimerPeSecunda;

void canta(float frecventa, int volum) {
  uint32_t perioada = pasiTimerPeSecunda / frecventa;
  buzzer.period_raw(perioada);
  buzzer.pulseWidth_raw(perioada * volum / 200);
}

void liniste() {
  buzzer.pulseWidth_raw(0);
}

void setup() {
  Serial.begin(115200);
  buzzer.begin(FRECVENTA_REFERINTA, 0.0);
  pasiTimerPeSecunda = buzzer.get_timer()->get_period_raw() * FRECVENTA_REFERINTA;
}

void loop() {
  Serial.println("Gama Do major");
  for (int i = 0; i < 8; i++) {
    Serial.print("  ");
    Serial.print(NUME_NOTE[i]);
    Serial.print(" - ");
    Serial.print(GAMA[i]);
    Serial.println(" Hz");
    canta(GAMA[i], 100);
    delay(400);
  }
  liniste();
  delay(500);

  Serial.println("Glisare C4 -> C6 -> C4");
  for (float f = 261.63; f <= 1046.5; f *= 1.01) {
    canta(f, 100);
    delay(15);
  }
  for (float f = 1046.5; f >= 261.63; f /= 1.01) {
    canta(f, 100);
    delay(15);
  }
  liniste();
  delay(500);

  Serial.println("Nota A4 (440 Hz), volum 100% -> 10%");
  for (int volum = 100; volum >= 10; volum -= 30) {
    Serial.print("  volum ");
    Serial.print(volum);
    Serial.println("%");
    canta(440, volum);
    delay(700);
  }
  liniste();
  delay(1500);
}
