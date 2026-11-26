// Test HC-SR04: Trig D4, Echo D2. Afiseaza distanta in Serial Monitor.

#include <Arduino.h>

const int PIN_TRIG = 4;
const int PIN_ECHO = 2;
const unsigned long ASTEPTARE_ECOU_US = 25000;

// Returneaza distanta in cm, sau -1 daca nu vine ecoul
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

void setup() {
  Serial.begin(115200);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  Serial.println("Test HC-SR04");
}

void loop() {
  float distanta = citesteDistanta();
  if (distanta < 0) {
    Serial.println("---");
  } else {
    Serial.print(distanta, 1);
    Serial.println(" cm");
  }
  delay(200);
}
