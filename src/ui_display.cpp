#include "ui_display.h"
#include "config.h"
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <math.h>

namespace {
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
const char *NOTE_NAMES[12] = {"C",  "C#", "D",  "D#", "E",  "F",
                              "F#", "G",  "G#", "A",  "A#", "B"};

float semitonesFromA4(float freqHz) {
    return 12.0f * log2f(freqHz / static_cast<float>(FREQ_A4));
}
} // namespace

namespace UIDisplay {
void begin() {
    Wire.begin();
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Theremin SIO R4 ");
}

void render(bool pitchActive, float freqHz, float pitchDistCm,
            int volumePercent, bool noteMode) {
    char line1[17];
    char line2[17];

    if (pitchActive) {
        int midiNote = MIDI_A4 + lroundf(semitonesFromA4(freqHz));
        char noteStr[5];
        snprintf(noteStr, sizeof(noteStr), "%s%d", NOTE_NAMES[midiNote % 12],
                 (midiNote / 12) - 1);
        int tenthsHz = static_cast<int>(freqHz * 10.0f + 0.5f);
        snprintf(line1, sizeof(line1), "%-4s %4d.%d Hz  ", noteStr,
                 tenthsHz / 10, tenthsHz % 10);
    } else {
        snprintf(line1, sizeof(line1), "-- fara mana -- ");
    }

    snprintf(line2, sizeof(line2), "Vol %3d%%    %s", volumePercent,
             noteMode ? "NOTE" : "CONT");

    lcd.setCursor(0, 0);
    lcd.print(line1);
    lcd.setCursor(0, 1);
    lcd.print(line2);

    Serial.print(line1);
    Serial.print(" | ");
    Serial.print(line2);
    if (pitchActive) {
        Serial.print(" | ");
        Serial.print(pitchDistCm, 1);
        Serial.print(" cm");
    }
    Serial.println();
}
} // namespace UIDisplay
