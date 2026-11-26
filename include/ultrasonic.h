#ifndef __ULTRASONIC_H__
#define __ULTRASONIC_H__

#include <Arduino.h>

/**
 * Bare-metal driver for two HC-SR04 sensors using RA4M1 hardware registers
 * (R_PORT1, R_AGT1, and R_ICU/NVIC). Alternates trigger pulses so the two
 * 40 kHz acoustic cones never interfere with each other.
 */
class UltrasonicPair {
  public:
    void begin();
    void update();

    float getPitchDistanceCm() const;
    float getVolumeDistanceCm() const;
    bool isPitchHandPresent() const;
    bool isVolumeHandPresent() const;
};

#endif // __ULTRASONIC_H__
