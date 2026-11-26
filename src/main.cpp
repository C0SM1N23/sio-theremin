#include "audio_engine.h"
#include "config.h"
#include "ui_display.h"
#include "ultrasonic.h"
#include <Arduino.h>
#include <math.h>

UltrasonicPair sensors;

bool noteMode = false;
bool wasPitchPresent = false;
int currentNoteIndex = 0;
float currentFreqHz = FREQ_MIN_HZ;
int currentVolume = VOL_DEFAULT;

bool prevBtnPressed = false;
unsigned long lastBtnTime = 0;
unsigned long lastUiTime = 0;

// Maps hand distance exponentially across NUM_OCTAVES (closer hand = higher
// pitch).
float distanceToFrequency(float distCm) {
    distCm = constrain(distCm, DIST_MIN_CM, DIST_MAX_CM);
    float octavesUp =
        NUM_OCTAVES * (DIST_MAX_CM - distCm) / (DIST_MAX_CM - DIST_MIN_CM);
    return FREQ_MIN_HZ * powf(2.0f, octavesUp);
}

int distanceToVolume(float distCm) {
    distCm = constrain(distCm, VOL_DIST_MIN_CM, VOL_DIST_MAX_CM);
    float ratio =
        (distCm - VOL_DIST_MIN_CM) / (VOL_DIST_MAX_CM - VOL_DIST_MIN_CM);
    return static_cast<int>(lroundf(ratio * 100.0f));
}

void updateModeButton() {
    bool pressed = (digitalRead(PIN_MODE_BTN) == LOW);
    if (pressed != prevBtnPressed &&
        (millis() - lastBtnTime) > BTN_DEBOUNCE_MS) {
        lastBtnTime = millis();
        prevBtnPressed = pressed;
        if (pressed) {
            noteMode = !noteMode;
            AudioEngine::setGlideShift(noteMode ? GLIDE_NOTE : GLIDE_CONT);
        }
    }
}

/**
 * Updates synthesizer pitch and volume from the two ultrasonic sensors.
 * In NOTE mode, hysteresis (NOTE_HYSTERESIS) prevents rapid trilling when the
 * player's hand hovers near a semitone boundary.
 */
void updateThereminLogic() {
    bool pitchPresent = sensors.isPitchHandPresent();
    bool justAppeared = (pitchPresent && !wasPitchPresent);
    wasPitchPresent = pitchPresent;

    if (pitchPresent) {
        float rawFreq = distanceToFrequency(sensors.getPitchDistanceCm());

        if (noteMode) {
            float semitones =
                12.0f * log2f(rawFreq / static_cast<float>(FREQ_A4));
            if (justAppeared ||
                fabsf(semitones - currentNoteIndex) > NOTE_HYSTERESIS) {
                currentNoteIndex = lroundf(semitones);
            }
            currentFreqHz = static_cast<float>(FREQ_A4) *
                            powf(2.0f, currentNoteIndex / 12.0f);
        } else {
            currentFreqHz = rawFreq;
        }

        AudioEngine::setFrequency(currentFreqHz);

        if (sensors.isVolumeHandPresent()) {
            currentVolume = distanceToVolume(sensors.getVolumeDistanceCm());
        } else {
            currentVolume = VOL_DEFAULT;
        }
        AudioEngine::setVolume(currentVolume);
    } else {
        AudioEngine::setVolume(0);
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(PIN_MODE_BTN, INPUT_PULLUP);

    UIDisplay::begin();
    sensors.begin();

    if (!AudioEngine::begin()) {
        Serial.println("Eroare: nu am gasit un timer liber pentru sunet");
    }
    delay(600);
}

void loop() {
    updateModeButton();
    sensors.update();
    updateThereminLogic();

    if (millis() - lastUiTime >= UI_PERIOD_MS) {
        lastUiTime = millis();
        UIDisplay::render(sensors.isPitchHandPresent(), currentFreqHz,
                          sensors.getPitchDistanceCm(), currentVolume,
                          noteMode);
    }
}
