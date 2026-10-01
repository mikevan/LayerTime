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

#include "DisplayGate.h"

namespace layertime {
namespace twatch_s3plus {
namespace ui {

bool DoubleTap::tap(uint32_t nowMs)
{
    if (_pending && nowMs - _firstMs <= kDoubleTapWindowMs) {
        _pending = false;
        return true;
    }
    _pending = true;
    _firstMs = nowMs;
    return false;
}

void DisplayGate::sleepNow()
{
    if (_state == State::Awake) _sleepRequested = true;
}

DisplayGate::Action DisplayGate::update(uint32_t inactiveMs, bool touchDown)
{
    switch (_state) {
    case State::Awake:
        if (_sleepRequested) {
            _sleepRequested = false;
            _state = State::Dark;
            _chosenDark = true;
            _sawRelease = false;
            return Action::Blank;
        }
        if (inactiveMs >= kDisplayTimeoutMs) {
            _state = State::Dark;
            _chosenDark = false;
            return Action::Blank;
        }
        return Action::None;

    case State::Dark:
        if (_chosenDark) {
            // Only a new touch ends a chosen dark; activity (an alert) does
            // not.
            if (!touchDown) {
                _sawRelease = true;
                return Action::None;
            }
            if (!_sawRelease) return Action::None;
            _chosenDark = false;
            _state = State::WaitRelease;
            return Action::Wake;
        }
        if (touchDown) {
            _state = State::WaitRelease;
            return Action::Wake;
        }
        // Something other than touch marked activity: an alert.
        if (inactiveMs < kDisplayTimeoutMs) {
            _state = State::Awake;
            return Action::WakeAndInput;
        }
        return Action::None;

    case State::WaitRelease:
        if (!touchDown) {
            _state = State::Awake;
            return Action::Input;
        }
        return Action::None;
    }
    return Action::None;
}

} // namespace ui
} // namespace twatch_s3plus
} // namespace layertime
