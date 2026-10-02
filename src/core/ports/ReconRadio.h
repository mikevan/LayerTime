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

// Port: the radio primitives Recon scheduling drives.
//
// Slice 1 Increment 2A expressed the Recon scheduling policy (channel
// hopping, the Wi-Fi/BLE alternation, the early-warning duty cycle) in core
// (logic/ReconScheduler), taken from the T-Watch Ultra reference. What a
// platform supplies is this: six primitives on the physical radios. The
// scheduler never sees a driver; a platform implements these six calls and
// keeps its settle delays, driver handles and callback plumbing to itself.
// The T-Dongle-C5 (platform/tdongle_c5/C5ReconRadio) is the implementation.
//
// Frame and advertisement acquisition is not part of this port. A platform
// hands what its radios receive to the core classifiers itself
// (logic/WifiFrameClassifier, logic/BleAdvertClassifier) and delivers the
// candidates through MonitorSource::setCandidateSink.

#include <stdint.h>

#include "../model/MonitorEvent.h"

namespace layertime {
namespace recon {

class ReconRadio {
public:
    virtual ~ReconRadio() = default;

    // Puts the Wi-Fi radio into promiscuous receive on channel 1 with the
    // platform's frame callback attached. The scheduler resets its hop
    // cursor to 1 when it calls this. Safe to call while already monitoring.
    virtual void startWifiMonitoring() = 0;
    // Leaves promiscuous receive and detaches the frame callback. Safe to
    // call when not monitoring.
    virtual void stopWifiMonitoring() = 0;
    // Moves the promiscuous receiver to a 2.4 GHz channel (1 to 11).
    virtual void setWifiChannel(uint8_t channel) = 0;

    // Starts one passive BLE scan of durationMs for `detector` (the
    // selection whose BLE members the scan reports). The platform stops
    // Wi-Fi monitoring first, as the reference always did. Adverts stream
    // to the platform's own callback while the scan runs.
    virtual void startBleScan(ReconTarget detector, uint32_t durationMs) = 0;
    // Stops an in-flight scan. Safe to call when none is running or the
    // BLE stack was never started.
    virtual void stopBleScan() = 0;
    // True while a scan started by startBleScan is still running.
    virtual bool bleScanning() const = 0;
};

} // namespace recon
} // namespace layertime
