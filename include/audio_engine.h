#ifndef __AUDIO_ENGINE_H__
#define __AUDIO_ENGINE_H__

#include <Arduino.h>

/**
 * 48 kHz DDS sine synthesizer driving the RA4M1 12-bit DAC (A0) via FspTimer.
 * Thread-safe setters can be called freely from loop() without disabling
 * interrupts.
 */
namespace AudioEngine {
bool begin();
void setFrequency(float freqHz);
void setVolume(int volumePercent);
void setGlideShift(int shift);
} // namespace AudioEngine

#endif // __AUDIO_ENGINE_H__
