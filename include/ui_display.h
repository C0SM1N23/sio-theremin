#ifndef __UI_DISPLAY_H__
#define __UI_DISPLAY_H__

#include <Arduino.h>

namespace UIDisplay {
void begin();
void render(bool pitchActive, float freqHz, float pitchDistCm,
            int volumePercent, bool noteMode);
} // namespace UIDisplay

#endif // __UI_DISPLAY_H__
