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

// A detection a classifier wants reported, before it becomes a log record.
// Shared by the Wi-Fi and BLE classifiers. Pointers are only valid for the
// duration of the sink call; the receiver copies what it keeps.

#include <stdint.h>

#include "../model/MonitorEvent.h"

namespace layertime {
namespace recon {

struct Candidate {
    ReconTarget detector = ReconTarget::None;
    const char *detail = nullptr;
    const char *address = nullptr;
    int8_t rssi = 0;
    Confidence confidence = Confidence::High;
    uint8_t channel = 0;
};

// Receives each candidate, in the order the classifier finds them.
using CandidateSink = void (*)(const Candidate &candidate, void *context);

// Asked before each detector is evaluated. The classifier skips a detector,
// and any state that detector would update, when this returns false.
using WantsFn = bool (*)(ReconTarget detector, const void *context);

} // namespace recon
} // namespace layertime
