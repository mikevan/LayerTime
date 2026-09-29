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

// Port: where a platform persists new Recon events, if it does.

#include "../model/MonitorEvent.h"

namespace layertime {

class EventLog {
public:
    virtual ~EventLog() = default;
    // Called once per newly created event, never for a repeat sighting. May
    // be called from a radio callback task.
    virtual void append(const MonitorEvent &event) = 0;
};

} // namespace layertime
