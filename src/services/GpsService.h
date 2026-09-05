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

#include <stdint.h>

#include "../model/WatchState.h"
#include "UbxParser.h"

class GpsService {
public:
    void begin(bool enabled);
    void setEnabled(bool enabled);
    void poll(WatchState &state);
    bool enabled() const { return _enabled; }

private:
    void applyEnabled(bool enabled);

    // Reads the GNSS UART once and hands every byte to BOTH parsers. The NMEA
    // side is byte-for-byte what GPS::loop() did before; the UBX side is new.
    // Doing it here rather than calling instance.gps.loop() is what stops the
    // two parsers fighting over the same port.
    void pumpSerial();

    // Asks the receiver for one NAV-PVT, about once a second. A poll writes
    // nothing to the receiver's configuration, so there is nothing to persist
    // and nothing to undo.
    void pollIfDue();

    void resetFixTracking();

    bool _enabled = true; // LilyGoLib powers/initializes GNSS during instance.begin().

    Ubx::Parser _ubx;
    Ubx::NavPvt _lastPvt;
    bool _havePvt = false;
    bool _everHadFix = false;
    uint32_t _lastUsableFixMs = 0;
    uint32_t _lastPollMs = 0;
};
