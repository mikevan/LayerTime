// LayerTime - counter-intrusion and resilient-communications firmware
// for the LilyGo T-Watch Ultra.
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

// Port: how the platform interrupts the wearer. Core decides WHETHER to
// alert; the platform decides HOW (vibrate, wake the screen).

#include "../model/Alert.h"

namespace layertime {

class AlertSink {
public:
    virtual ~AlertSink() = default;
    // Called on the application loop, once per alert.
    virtual void raise(const Alert &alert) = 0;
};

} // namespace layertime
