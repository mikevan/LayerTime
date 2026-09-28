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

// Which detectors a Recon selection covers, which radio each one needs, and
// the names shown for them. Moved unchanged out of ReconService in Phase 0
// Step 3c.
//
// Selections, as offered by the Recon menu:
//   * Group sweeps (Trackers, CounterSurveil, CounterIntrusion) scan every
//     detector in the group. Detections are always logged under the
//     individual detector that matched, never the group, so the threat log
//     reads FLOCK or AIRTAG rather than TRACKERS.
//   * EarlyWarning is not user-selectable. It scopes the background
//     early-warning BLE scan to Flipper and Meta only.

#include <stddef.h>

#include "../model/MonitorEvent.h"

namespace layertime {
namespace recon {

// Group membership. Single source of truth for both the radio scheduling
// and the Recon menu's sub-pages: add a detector here and it is picked up
// by the sweep and the UI at once.
const ReconTarget *groupMembers(ReconTarget group, size_t &count);
bool groupContains(ReconTarget group, ReconTarget detector);

// Which radios a single detector can be found on. Deliberately two
// independent predicates rather than one either/or: Flock is genuinely
// both, a BLE manufacturer/OUI match AND a Wi-Fi beacon BSSID match, and
// deriving Wi-Fi as "not BLE" would have silently disabled its beacon path
// whenever FLOCK was selected on its own.
bool isBleDetector(ReconTarget detector);
bool isWifiDetector(ReconTarget detector);

// Whether a selection (a detector, a group, or All) needs that radio.
bool needsBle(ReconTarget selection);
bool needsWifi(ReconTarget selection);

// Whether a BLE scan started for `scanSelection` reports `target`.
bool bleScanWants(ReconTarget scanSelection, ReconTarget target);

// The Wi-Fi detectors the background early-warning sweep listens for.
bool isBackgroundWifiDetector(ReconTarget detector);

// Whether a detector should report right now. During a manual session:
// the selection covers it. Otherwise: only while the early-warning sweep is
// actively sweeping, and only for the background Wi-Fi detectors.
bool wants(bool monitoring, ReconTarget selection, bool earlyWarningSweeping,
           ReconTarget detector);

const char *detectorName(ReconTarget detector);
const char *detectorShortName(ReconTarget detector);
const char *confidenceLabel(Confidence confidence);

} // namespace recon
} // namespace layertime
