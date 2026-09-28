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

#include "AlertPolicy.h"

namespace layertime {
namespace alert {

bool raisesOnNewRecord(Confidence confidence, bool sleepMode)
{
    return !sleepMode && confidence != Confidence::Low;
}

RepeatOutcome onRepeatSighting(Confidence recorded, Confidence seen)
{
    RepeatOutcome out;
    out.confidence = seen > recorded ? seen : recorded;
    out.raiseAlert = false;
    return out;
}

} // namespace alert
} // namespace layertime
