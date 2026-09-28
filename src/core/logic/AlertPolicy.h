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

// When a Recon detection interrupts the wearer. Moved unchanged out of
// ReconService::addDetection in Phase 0 Step 3g. This is the alert policy
// exactly as characterized by test_recon, including its known defect.
//
// Every detection is logged and counted either way. This only decides
// whether the watch buzzes and pops up for it.

#include "../model/MonitorEvent.h"

namespace layertime {
namespace alert {

// A newly recorded emitter raises the alert unless sleep mode is on or the
// match is Low confidence. Low matches are logged and counted like anything
// else but deliberately never alert: the generic-Espressif Flock prefixes
// would otherwise buzz the wrist for every ESP32 in range, which is how a
// detector gets ignored. Sleep mode uses the same mechanism: nothing is
// missed, only the interruption is suppressed.
bool raisesOnNewRecord(Confidence confidence, bool sleepMode);

struct RepeatOutcome {
    // What the record's confidence becomes: the strongest grade this
    // emitter has ever matched at. A later Low match never downgrades a
    // High one.
    Confidence confidence;
    // Always false. KNOWN DEFECT, carried over unchanged: a repeat sighting
    // never raises the alert, even when it upgrades the record from Low to
    // High, so an emitter first seen at Low never buzzes. Pinned by
    // test_recon: low_first_then_high_never_alerts_KNOWN_DEFECT.
    bool raiseAlert;
};

RepeatOutcome onRepeatSighting(Confidence recorded, Confidence seen);

} // namespace alert
} // namespace layertime
