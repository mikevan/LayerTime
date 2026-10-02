// LayerTime - counter-intrusion and resilient-communications firmware
// for the LilyGo T-Watch S3 Plus.
//
// Copyright (C) 2026 Michael Van Geertruy
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

// When the S3 Plus display sleeps and wakes. Pure, tested with g++
// (devices/lilygo-s3plus/test/test_s3plus_display).
//
// The backlight goes off after kDisplayTimeoutMs without input. While it is
// off, touch input is not handed to the screens at all, so a tap on a dark
// watch can never press a button it cannot see (Clear, a detector, Dismiss).
// A tap only wakes the display; input is handed back once that finger lifts.
// An alert wakes the display directly (the alert sink marks activity) and
// hands input back at once, so Dismiss works on the first tap.
//
// A double-tap on the watch face puts the display to sleep at once
// (sleepNow). That sleep is the wearer's choice of dark, for a meeting or
// for light discipline, so only the wearer's own touch ends it: an alert
// still buzzes but does not light the screen. The touch that ends it must
// be a new one: the finger that made the double-tap has to lift first.
//
// The touch controller itself stays powered the whole time: its reset line is
// not connected on this board, so a controller put to sleep would not come
// back.

#include <stdint.h>

namespace layertime {
namespace twatch_s3plus {
namespace ui {

constexpr uint32_t kDisplayTimeoutMs = 15000;
// Two taps on the watch face within this long put the display to sleep.
// Same window as the T-Ultra (WatchApp kDoubleTapWindowMs).
constexpr uint32_t kDoubleTapWindowMs = 500;

// Pairs taps into a double-tap. Pure, so it is tested with g++.
class DoubleTap {
public:
    // One tap at nowMs. True when it completes a double-tap; the pair is
    // then used up, so a third quick tap starts a new pair.
    bool tap(uint32_t nowMs);
    void reset() { _pending = false; }

private:
    bool _pending = false;
    uint32_t _firstMs = 0;
};

class DisplayGate {
public:
    enum class Action : uint8_t {
        None,
        Blank,        // backlight off, stop handing input to the screens
        Wake,         // backlight on, keep input withheld until the finger lifts
        WakeAndInput, // backlight on and hand input back (activity woke it)
        Input,        // the waking finger lifted: hand input back
    };

    // inactiveMs: time since the screens last saw input or activity.
    // touchDown: the raw touch controller reports a finger right now.
    Action update(uint32_t inactiveMs, bool touchDown);
    // The double-tap: blank on the next update. Ignored unless awake.
    void sleepNow();

    bool lit() const { return _state != State::Dark; }
    bool inputEnabled() const { return _state == State::Awake; }

private:
    enum class State : uint8_t { Awake, Dark, WaitRelease };
    State _state = State::Awake;
    bool _sleepRequested = false;
    bool _chosenDark = false;  // dark by double-tap, not by timeout
    bool _sawRelease = false;  // no finger on the panel since going dark
};

} // namespace ui
} // namespace twatch_s3plus
} // namespace layertime
