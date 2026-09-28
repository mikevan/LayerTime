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

// Canonical definition: contracts/models.md, "Alert".

#include <stdint.h>

#include "Time.h"

namespace layertime {

// Numeric values are part of the contract. Do not renumber; append only.
// Recon detection is the only alert the T-Ultra raises today. Nothing else
// is added until something actually raises it.
enum class AlertKind : uint8_t {
    ReconDetection = 0,
};

// Something the wearer should be interrupted for. The core decides WHETHER
// to alert. The platform decides HOW (vibrate, wake the screen, popup).
//
// Suppression policy, characterized from the T-Ultra and binding on every
// platform: an event is always logged and counted, but raises no alert
// when sleep mode is on or when the match is Low confidence.
struct Alert {
    AlertKind kind = AlertKind::ReconDetection;
    // The MonitorEvent that caused it.
    uint32_t eventId = 0;
    Timestamp raised;
};

} // namespace layertime
