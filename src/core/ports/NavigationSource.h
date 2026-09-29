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

// Port: where the wearer's position comes from.

#include "../model/NavigationState.h"

namespace layertime {

class NavigationSource {
public:
    virtual ~NavigationSource() = default;
    // Fills every field. A quantity the platform cannot measure keeps its
    // validity flag false.
    virtual void read(NavigationState &out) = 0;
};

} // namespace layertime
