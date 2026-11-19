// Test LCD 1602 I2C: SDA A4, SCL A5. Cauta adresa pe I2C, apoi scrie un text.

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const uint8_t ADRESA_LCD = 0x27;  // sau 0x3F

LiquidCrystal_I2C lcd(ADRESA_LCD, 16, 2);

void scaneazaI2C() {
  Serial.println("Caut dispozitive I2C...");
  uint8_t gasite = 0;
  for (uint8_t adr = 1; adr < 127; adr++) {
    Wire.beginTransmission(adr);
    if (Wire.endTransmission() == 0) {
      Serial.print("  gasit la adresa 0x");
      Serial.println(adr, HEX);
      gasite++;
    }
  }
  if (gasite == 0) {
    Serial.println("  nimic gasit: verifica SDA -> A4, SCL -> A5, VCC si GND");
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  scaneazaI2C();

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Theremin SIO");
}

void loop() {
  lcd.setCursor(0, 1);
  lcd.print("Secunde: ");
  lcd.print(millis() / 1000);
  delay(500);
}
