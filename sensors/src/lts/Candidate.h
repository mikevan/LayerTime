// LayerTime-Sensors - passive wireless threat detectors for small radios.
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

// What a classifier reports: one detector matched one transmitter. Pointers
// are only valid for the duration of the sink call; the receiver copies what
// it keeps. Which radio and band saw it, and when, are known only to the
// caller, so they are not part of a candidate.

#include <stdint.h>

#include "Detectors.h"

namespace lts {

struct Candidate {
    DetectorId detector = DetectorId::Deauth;
    const char *detail = nullptr;
    const char *address = nullptr;
    int8_t rssi = 0;
    Confidence confidence = Confidence::High;
    // Wi-Fi channel the frame was captured on; 0 for BLE.
    uint8_t channel = 0;
};

// Receives each candidate, in the order the classifier finds them.
using CandidateSink = void (*)(const Candidate &candidate, void *context);

} // namespace lts
