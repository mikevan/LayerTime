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

// The S3 Plus GNSS service: the u-blox MIA-M10Q on UART1 (RX 41, TX 42,
// 38400 baud), powered from AXP2101 BLDO1. LilyGoLib's begin() powers it and
// opens Serial1; nothing else reads that port (LilyGoLib's own loop() does
// not), so this service owns it.
//
// An S3 Plus copy of the T-Ultra's GpsService, with intentional differences:
//   * Position comes from UBX NAV-PVT only. The Ultra takes position from
//     NMEA through TinyGPSPlus and adds UBX for accuracy; the S3 Plus needs
//     neither NMEA altitude nor speed for Recon, so NMEA bytes are read and
//     dropped, and the Ultra's $PUBX GGA re-enable command is not sent.
//   * A fix is usable by gnss::pvtIsUsablePosition (time-only solutions are
//     not positions) and current by gnss::positionIsCurrent (age, never 0,0).
//   * NAV-PVT's UTC date and time are kept for the clock.
//
// NAV-PVT is polled once a second, as on the Ultra. A poll writes nothing to
// the receiver's configuration.

#include <stdint.h>

#include "GnssRules.h"
#include "S3PlusUbx.h"

namespace layertime {
namespace twatch_s3plus {

class S3PlusGpsService {
public:
    // The Settings GPS switch: powers the receiver's rail (AXP2101 BLDO1) on
    // or off. Off throws away everything known about the fix, as on the
    // Ultra: a position from a receiver that is no longer running is a claim
    // that cannot be supported. Call once at boot, then on each change.
    void setEnabled(bool enabled);
    bool enabled() const { return _enabled; }

    // Reads the port and polls the receiver. Call every loop pass.
    void poll(uint32_t nowMs);

    bool havePvt() const { return _havePvt; }
    // The most recent NAV-PVT, usable or not.
    const ubx::NavPvt &lastPvt() const { return _lastPvt; }
    uint32_t lastPvtMs() const { return _lastPvtMs; }

    bool everHadFix() const { return _everHadFix; }
    // Since the last usable position; 0 before the first.
    uint32_t fixAgeMs(uint32_t nowMs) const { return _everHadFix ? nowMs - _lastUsableFixMs : 0; }
    bool positionCurrent(uint32_t nowMs) const
    {
        return gnss::positionIsCurrent(_everHadFix, fixAgeMs(nowMs));
    }
    // The last usable position (kept while a newer frame has no fix).
    const ubx::NavPvt &lastUsablePvt() const { return _lastUsablePvt; }

    uint32_t framesAccepted() const { return _ubx.framesAccepted(); }
    uint32_t framesRejected() const { return _ubx.framesRejected(); }

private:
    void pumpSerial(uint32_t nowMs);
    void pollIfDue(uint32_t nowMs);

    ubx::Parser _ubx;
    ubx::NavPvt _lastPvt;
    ubx::NavPvt _lastUsablePvt;
    bool _havePvt = false;
    uint32_t _lastPvtMs = 0;
    bool _everHadFix = false;
    uint32_t _lastUsableFixMs = 0;
    uint32_t _lastPollMs = 0;
    bool _enabled = true;
};

} // namespace twatch_s3plus
} // namespace layertime
