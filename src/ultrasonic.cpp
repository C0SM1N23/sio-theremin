#include "ultrasonic.h"
#include "config.h"

namespace {
constexpr uint16_t MASK_PITCH_TRIG = (1U << 4);  // P104 (D4)
constexpr uint16_t MASK_VOL_TRIG = (1U << 5);    // P105 (D5)
constexpr uint16_t MASK_PITCH_ECHO = (1U << 11); // P111 (D2)
constexpr uint16_t MASK_VOL_ECHO = (1U << 12);   // P112 (D3)

constexpr uint32_t EVENT_ICU_IRQ4 = 0x05;
constexpr uint32_t EVENT_ICU_IRQ5 = 0x06;

struct EchoChannel {
    volatile uint16_t startTick;
    volatile uint16_t pulseTicks;
    volatile bool newSample;

    bool handPresent;
    int misses;
    float prev2;
    float prev1;
    float smoothed;
};

EchoChannel pitchCh = {
    0,           0,           false,      false, MISSES_TO_SILENCE,
    NO_DISTANCE, NO_DISTANCE, NO_DISTANCE};
EchoChannel volCh = {
    0,           0,           false,      false, MISSES_TO_SILENCE,
    NO_DISTANCE, NO_DISTANCE, NO_DISTANCE};

int8_t ielsrSlotPitch = -1;
int8_t ielsrSlotVol = -1;

unsigned long lastTriggerMs = 0;
bool triggerPitchNext = true;

/**
 * Edge-triggered echo ISRs.
 * Captures R_AGT1->AGT on rising/falling edges (unsigned subtraction handles
 * 16-bit counter wrap-around automatically) and clears the ICU IR flag
 * manually.
 */
void isrPitchEcho() {
    uint16_t now = R_AGT1->AGT;
    if (R_PORT1->PIDR & MASK_PITCH_ECHO) {
        pitchCh.startTick = now;
    } else {
        pitchCh.pulseTicks = static_cast<uint16_t>(pitchCh.startTick - now);
        pitchCh.newSample = true;
    }
    R_ICU->IELSR_b[ielsrSlotPitch].IR = 0;
}

void isrVolEcho() {
    uint16_t now = R_AGT1->AGT;
    if (R_PORT1->PIDR & MASK_VOL_ECHO) {
        volCh.startTick = now;
    } else {
        volCh.pulseTicks = static_cast<uint16_t>(volCh.startTick - now);
        volCh.newSample = true;
    }
    R_ICU->IELSR_b[ielsrSlotVol].IR = 0;
}

/**
 * Starts AGT1 as a free-running 16-bit down-counter clocked at PCLKB/8 (3 MHz).
 * Gives 0.333 us resolution and a 21.8 ms wrap-around window—well above
 * ECHO_TIMEOUT_US.
 */
void initTimerAGT1() {
    R_MSTP->MSTPCRD &= ~(1U << 2);

    R_AGT1->AGTCR = 0x00;
    while (R_AGT1->AGTCR_b.TCSTF != 0) {
    }

    R_AGT1->AGTMR1 = (1U << 4);
    R_AGT1->AGTMR2 = 0x00;
    R_AGT1->AGT = 0xFFFF;
    R_AGT1->AGTCR = 0x01;
}

/**
 * Claims an unused NVIC/IELSR slot in the RA4M1 Interrupt Controller Unit,
 * configures both-edge detection with digital noise filtering, and registers
 * the ISR directly in the Cortex-M4 RAM vector table (SCB->VTOR).
 */
int8_t attachRawIrq(uint8_t irqNum, uint32_t eventCode, void (*handler)(),
                    uint8_t priority) {
    R_ICU->IRQCR[irqNum] = (1U << 7) | 0x03;

    for (int8_t slot = 0; slot < 32; slot++) {
        if ((R_ICU->IELSR[slot] & 0xFFU) == 0) {
            uint32_t *vectorTable = reinterpret_cast<uint32_t *>(SCB->VTOR);
            vectorTable[16 + slot] = reinterpret_cast<uint32_t>(handler);

            R_ICU->IELSR[slot] = eventCode;
            R_ICU->IELSR_b[slot].IR = 0;

            IRQn_Type nvicIrq = static_cast<IRQn_Type>(slot);
            NVIC_SetPriority(nvicIrq, priority);
            NVIC_ClearPendingIRQ(nvicIrq);
            NVIC_EnableIRQ(nvicIrq);
            return slot;
        }
    }
    return -1;
}

// Generates a 10 us trigger pulse via Port 1 Set/Reset registers and AGT1 ticks
// (30 ticks = 10 us).
void fireTriggerRegister(uint16_t trigMask) {
    R_PORT1->PORR = trigMask;
    uint16_t t0 = R_AGT1->AGT;
    while (static_cast<uint16_t>(t0 - R_AGT1->AGT) < 6U) {
    }

    R_PORT1->POSR = trigMask;
    t0 = R_AGT1->AGT;
    while (static_cast<uint16_t>(t0 - R_AGT1->AGT) < 30U) {
    }

    R_PORT1->PORR = trigMask;
}

float medianOfThree(float a, float b, float c) {
    if ((a <= b && b <= c) || (c <= b && b <= a))
        return b;
    if ((b <= a && a <= c) || (c <= a && a <= b))
        return a;
    return c;
}

/**
 * Rejects single-sample acoustic spikes via a 3-point median filter, then
 * applies an adaptive EMA whose alpha scales quadratically with hand velocity
 * so steady notes stay rock-solid while fast vibrato/glissando tracks
 * immediately.
 */
void processChannel(EchoChannel &ch, float maxValidCm) {
    float rawCm = NO_DISTANCE;

    if (ch.newSample) {
        noInterrupts();
        uint16_t ticks = ch.pulseTicks;
        ch.newSample = false;
        interrupts();

        float widthUs = ticks / 3.0f;
        if (widthUs > 0.0f && widthUs <= ECHO_TIMEOUT_US) {
            rawCm = widthUs / static_cast<float>(US_PER_CM);
        }
    }

    if (rawCm == NO_DISTANCE || rawCm > maxValidCm) {
        if (ch.misses < MISSES_TO_SILENCE)
            ch.misses++;
        if (ch.misses >= MISSES_TO_SILENCE) {
            ch.handPresent = false;
            ch.prev2 = NO_DISTANCE;
            ch.prev1 = NO_DISTANCE;
            ch.smoothed = NO_DISTANCE;
        }
        return;
    }

    ch.handPresent = true;
    ch.misses = 0;

    float clean = rawCm;
    if (ch.prev2 != NO_DISTANCE) {
        clean = medianOfThree(ch.prev2, ch.prev1, rawCm);
    }
    ch.prev2 = ch.prev1;
    ch.prev1 = rawCm;

    if (ch.smoothed == NO_DISTANCE) {
        ch.smoothed = clean;
        return;
    }

    float delta = constrain(clean - ch.smoothed, -MAX_JUMP_CM, MAX_JUMP_CM);
    float alpha = SMOOTH_MIN + SMOOTH_GAIN * delta * delta;
    if (alpha > 1.0f)
        alpha = 1.0f;
    ch.smoothed += alpha * delta;
}
} // namespace

void UltrasonicPair::begin() {
    // Unlock Pin Function Select (PFS) write-protection before configuring
    // P104/P105/P111/P112
    R_PMISC->PWPR_b.B0WI = 0;
    R_PMISC->PWPR_b.PFSWE = 1;

    R_PORT1->PORR = (MASK_PITCH_TRIG | MASK_VOL_TRIG);
    R_PFS->PORT[1].PIN[4].PmnPFS = (1U << 2); // Output
    R_PFS->PORT[1].PIN[5].PmnPFS = (1U << 2); // Output

    R_PFS->PORT[1].PIN[11].PmnPFS = (1U << 14); // Input + IRQ4
    R_PFS->PORT[1].PIN[12].PmnPFS = (1U << 14); // Input + IRQ5

    R_PMISC->PWPR_b.PFSWE = 0;
    R_PMISC->PWPR_b.B0WI = 1;

    initTimerAGT1();

    ielsrSlotPitch = attachRawIrq(4, EVENT_ICU_IRQ4, isrPitchEcho, 3);
    ielsrSlotVol = attachRawIrq(5, EVENT_ICU_IRQ5, isrVolEcho, 3);
}

void UltrasonicPair::update() {
    unsigned long now = millis();
    if (now - lastTriggerMs < MEASURE_PERIOD_MS)
        return;
    lastTriggerMs = now;

    // Harvest the echo from the sensor fired 25 ms ago, then trigger the
    // opposite sensor
    if (triggerPitchNext) {
        processChannel(volCh, VOL_DIST_MAX_CM);
        fireTriggerRegister(MASK_PITCH_TRIG);
    } else {
        processChannel(pitchCh, DIST_MAX_CM);
        fireTriggerRegister(MASK_VOL_TRIG);
    }
    triggerPitchNext = !triggerPitchNext;
}

float UltrasonicPair::getPitchDistanceCm() const { return pitchCh.smoothed; }
float UltrasonicPair::getVolumeDistanceCm() const { return volCh.smoothed; }
bool UltrasonicPair::isPitchHandPresent() const { return pitchCh.handPresent; }
bool UltrasonicPair::isVolumeHandPresent() const { return volCh.handPresent; }
