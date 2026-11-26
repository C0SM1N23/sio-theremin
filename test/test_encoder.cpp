// Test encoder: CLK D8, DT D9, SW D7. Afiseaza pozitia si apasarile.
// La un clic pozitia trebuie sa se schimbe cu 1. Daca sare cu 2, PASI_PE_CLIC = 4 (si in main.cpp).

#include <Arduino.h>

const int PIN_ENCODER_CLK = 8;
const int PIN_ENCODER_DT = 9;
const int PIN_ENCODER_BUTON = 7;
const int PASI_PE_CLIC = 2;

volatile int schimbariCLK = 0;
int pozitie = 0;
bool butonApasatInainte = false;

void laSchimbareCLK() {
  if (digitalRead(PIN_ENCODER_CLK) != digitalRead(PIN_ENCODER_DT)) schimbariCLK++;
  else schimbariCLK--;
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_ENCODER_CLK, INPUT_PULLUP);
  pinMode(PIN_ENCODER_DT, INPUT_PULLUP);
  pinMode(PIN_ENCODER_BUTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_CLK), laSchimbareCLK, CHANGE);
  Serial.println("Test encoder");
}

void loop() {
  noInterrupts();
  int schimbari = schimbariCLK;
  schimbariCLK = schimbari % PASI_PE_CLIC;
  interrupts();

  int clicuri = schimbari / PASI_PE_CLIC;
  if (clicuri != 0) {
    pozitie += clicuri;
    Serial.print("Pozitie: ");
    Serial.println(pozitie);
  }

  bool apasat = (digitalRead(PIN_ENCODER_BUTON) == LOW);
  if (apasat && !butonApasatInainte) Serial.println("buton apasat");
  butonApasatInainte = apasat;

  delay(20);
}
