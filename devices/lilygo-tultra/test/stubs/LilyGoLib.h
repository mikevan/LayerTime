// TEST-ONLY stand-in for LilyGoLib's `instance`. Never part of the firmware
// build. Records the hardware calls characterization tests care about.

#pragma once

#include <stdint.h>

struct FakeLilyGoInstance {
    uint32_t vibrations = 0;
    uint8_t brightness = 255;
    void begin() {}
    void loop() {}
    void vibrator() { ++vibrations; }
    void setBrightness(uint8_t level) { brightness = level; }
    uint8_t getBrightness() const { return brightness; }
};

inline FakeLilyGoInstance instance;
