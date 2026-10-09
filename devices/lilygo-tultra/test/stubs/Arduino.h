// TEST-ONLY stand-in for the Arduino core. Never part of the firmware build.
//
// The clock is a variable the test sets, so time-dependent behaviour
// (windows, cooldowns, ages) is exact and repeatable.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace fake_arduino {
inline uint32_t g_millis = 0;
inline uint32_t g_delayCalls = 0;
}

inline uint32_t millis() { return fake_arduino::g_millis; }
inline void delay(uint32_t) { ++fake_arduino::g_delayCalls; }

struct FakeSerial {
    void begin(unsigned long) {}
    template <class... A> void printf(A...) {}
    template <class... A> void println(A...) {}
    template <class... A> void print(A...) {}
    explicit operator bool() const { return true; }
};
inline FakeSerial Serial;
