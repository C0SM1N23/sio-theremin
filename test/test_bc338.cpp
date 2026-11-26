// Varianta veche, fara amplificator: HC-SR04 (Trig D4, Echo D2) + difuzor prin BC338.
// D11 -> 1k -> baza, 10k baza-GND; 12V -> 68 ohm 5W -> difuzor -> colector; emitor -> GND;
// dioda pe difuzor; minusul de 12V la GND-ul Arduino. Nu se mai foloseste.

#include <Arduino.h>
#include <pwm.h>

const int PIN_TRIG = 4;
const int PIN_ECHO = 2;
const int PIN_DIFUZOR = 11;

const float DISTANTA_MIN_CM = 5.0;
const float DISTANTA_MAX_CM = 50.0;
const float FRECVENTA_MIN_HZ = 261.63;
const float NUMAR_OCTAVE = 2.0;

const unsigned long ASTEPTARE_ECOU_US = 25000;
const float FRECVENTA_REFERINTA = 200.0;

PwmOut difuzor(PIN_DIFUZOR);
float pasiTimerPeSecunda;
float distantaNetezita = -1;

float citesteDistanta() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  unsigned long durata = pulseIn(PIN_ECHO, HIGH, ASTEPTARE_ECOU_US);
  if (durata == 0) return -1;
  return durata / 58.0;
}

void canta(float frecventa) {
  uint32_t perioada = pasiTimerPeSecunda / frecventa;
  difuzor.period_raw(perioada);
  difuzor.pulseWidth_raw(perioada / 2);
}

void liniste() {
  difuzor.pulseWidth_raw(0);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);

  difuzor.begin(FRECVENTA_REFERINTA, 0.0);
  pasiTimerPeSecunda = difuzor.get_timer()->get_period_raw() * FRECVENTA_REFERINTA;
}

void loop() {
  float d = citesteDistanta();

  if (d < 0 || d > DISTANTA_MAX_CM) {
    liniste();
    distantaNetezita = -1;
    Serial.println("fara mana - liniste");
  } else {
    if (distantaNetezita < 0) distantaNetezita = d;
    else distantaNetezita = 0.7 * distantaNetezita + 0.3 * d;

    float dLimitat = constrain(distantaNetezita, DISTANTA_MIN_CM, DISTANTA_MAX_CM);
    float octaveUrcate = NUMAR_OCTAVE * (DISTANTA_MAX_CM - dLimitat) / (DISTANTA_MAX_CM - DISTANTA_MIN_CM);
    float frecventa = FRECVENTA_MIN_HZ * powf(2.0, octaveUrcate);
    canta(frecventa);

    Serial.print(distantaNetezita);
    Serial.print(" cm -> ");
    Serial.print(frecventa);
    Serial.println(" Hz");
  }

  delay(20);
}
