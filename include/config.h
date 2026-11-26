#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <Arduino.h>

// Arduino UNO R4 WiFi pin mapping.
// D2 (P111 -> IRQ4) and D3 (P112 -> IRQ5) are required because they map
// directly to external hardware interrupt channels in the Renesas RA4M1 ICU.
constexpr uint8_t PIN_PITCH_TRIG = 4; // P104
constexpr uint8_t PIN_PITCH_ECHO = 2; // P111 (IRQ4)
constexpr uint8_t PIN_VOL_TRIG = 5;   // P105
constexpr uint8_t PIN_VOL_ECHO = 3;   // P112 (IRQ5)
constexpr uint8_t PIN_MODE_BTN = 7;

constexpr uint8_t LCD_ADDR = 0x27;
constexpr uint8_t LCD_COLS = 16;
constexpr uint8_t LCD_ROWS = 2;

// Musical range & quantization
constexpr float DIST_MIN_CM = 5.0f;
constexpr float DIST_MAX_CM = 50.0f;
constexpr float FREQ_MIN_HZ = 261.63f; // C4
constexpr float NUM_OCTAVES = 2.0f;
constexpr double FREQ_A4 = 440.0;
constexpr int MIDI_A4 = 69;
constexpr float NOTE_HYSTERESIS =
    0.7f; // Semitones past center before snapping to adjacent note

// Volume sensor range (classic Theremin behavior: hand close = mute)
constexpr float VOL_DIST_MIN_CM = 5.0f;
constexpr float VOL_DIST_MAX_CM = 40.0f;
constexpr int VOL_DEFAULT = 40;

// Ultrasonic timing & adaptive smoothing
constexpr unsigned long MEASURE_PERIOD_MS =
    25; // Alternating period (each sensor updates at 20 Hz)
constexpr uint16_t ECHO_TIMEOUT_US =
    4500; // ~77 cm ceiling; ignores distant room reflections
constexpr double US_PER_CM = 58.0;
constexpr float NO_DISTANCE = -1.0f;
constexpr int MISSES_TO_SILENCE = 6;
constexpr float MAX_JUMP_CM = 8.0f;
constexpr float SMOOTH_MIN = 0.1f;
constexpr float SMOOTH_GAIN = 0.1f;

// 48 kHz Direct Digital Synthesis (DDS) audio engine
constexpr float SAMPLE_RATE_HZ = 48000.0f;
constexpr double PHASE_FULL_SCALE = 4294967296.0; // 2^32 phase accumulator
constexpr float PHASE_STEP_1HZ = PHASE_FULL_SCALE / SAMPLE_RATE_HZ;
constexpr int GLIDE_CONT =
    10; // Right-shift factor (/1024) for smooth portamento
constexpr int GLIDE_NOTE =
    7; // Right-shift factor (/128) for fast note transitions
constexpr uint8_t AUDIO_IRQ_PRIORITY =
    2; // Higher priority than ultrasonic IRQs (3)
constexpr int DAC_MIDPOINT = 2048;
constexpr int DAC_MAX_VAL = 4095;
constexpr int DC_OFFSET_SLOWDOWN =
    8; // Ramps 0 <-> 2048 in ~0.34 s to avoid speaker pop

constexpr unsigned long UI_PERIOD_MS = 400;
constexpr unsigned long BTN_DEBOUNCE_MS = 50;

#endif // __CONFIG_H__
