#include "audio_engine.h"
#include "config.h"
#include <FspTimer.h>
#include <math.h>

namespace {
FspTimer audioTimer;
int16_t sineTable[256];

volatile uint32_t targetStep = 0;
volatile int targetAmp = 0;
volatile int glideShift = GLIDE_CONT;

uint32_t phase = 0;
uint32_t currentStep = 0;
int currentAmp = 0;
int currentMidpoint = 0;
int midpointDivider = 0;

/**
 * 48 kHz sample synthesis ISR.
 *
 * Instead of holding the DAC at 2048 (2.5V) during silence—which couples 5V
 * rail noise from USB/Wi-Fi into the TDA2030A amplifier—the DC midpoint ramps
 * smoothly down to 0 when targetAmp is zero. Amplitude is clamped to
 * currentMidpoint / 2 during the ramp so the negative half-cycle never clips at
 * 0V.
 */
void audioISR(timer_callback_args_t *) {
    int ampGoal = targetAmp * 4;

    int midGoal = (ampGoal > 0) ? DAC_MIDPOINT : 0;
    if (++midpointDivider >= DC_OFFSET_SLOWDOWN) {
        midpointDivider = 0;
        if (currentMidpoint < midGoal)
            currentMidpoint++;
        else if (currentMidpoint > midGoal)
            currentMidpoint--;
    }

    int safeAmpGoal = ampGoal;
    if (safeAmpGoal > currentMidpoint / 2) {
        safeAmpGoal = currentMidpoint / 2;
    }

    if (currentAmp < safeAmpGoal)
        currentAmp++;
    else if (currentAmp > safeAmpGoal)
        currentAmp--;

    if (currentAmp == 0) {
        currentStep = targetStep;
    } else {
        int32_t diff = static_cast<int32_t>(targetStep) -
                       static_cast<int32_t>(currentStep);
        currentStep += (diff >> glideShift);
    }

    phase += currentStep;

    // 32-bit phase accumulator: top 8 bits index the LUT, next 8 bits
    // interpolate
    uint8_t idx = phase >> 24;
    uint8_t frac = (phase >> 16) & 0xFF;
    int a = sineTable[idx];
    int b = sineTable[(idx + 1) & 0xFF];
    int sample = a + (((b - a) * frac) >> 8);

    int dacVal = currentMidpoint + ((sample * currentAmp) >> 10);
    if (dacVal < 0)
        dacVal = 0;
    else if (dacVal > DAC_MAX_VAL)
        dacVal = DAC_MAX_VAL;

    R_DAC->DADR[0] = static_cast<uint16_t>(dacVal);
}
} // namespace

namespace AudioEngine {
bool begin() {
    for (int i = 0; i < 256; i++) {
        sineTable[i] = static_cast<int16_t>(
            lroundf(2047.0f * sinf(2.0f * PI * i / 256.0f)));
    }

    analogWriteResolution(12);
    analogWrite(DAC, 0);

    uint8_t timerType;
    int8_t channel = FspTimer::get_available_timer(timerType);
    if (channel < 0) {
        FspTimer::force_use_of_pwm_reserved_timer();
        channel = FspTimer::get_available_timer(timerType, true);
    }
    if (channel < 0)
        return false;

    audioTimer.begin(TIMER_MODE_PERIODIC, timerType, channel, SAMPLE_RATE_HZ,
                     0.0f, audioISR);
    audioTimer.setup_overflow_irq(AUDIO_IRQ_PRIORITY);
    audioTimer.open();
    audioTimer.start();
    return true;
}

void setFrequency(float freqHz) {
    targetStep = static_cast<uint32_t>(freqHz * PHASE_STEP_1HZ);
}

void setVolume(int volumePercent) {
    volumePercent = constrain(volumePercent, 0, 100);
    targetAmp = (volumePercent * 256) / 100;
}

void setGlideShift(int shift) { glideShift = shift; }
} // namespace AudioEngine
