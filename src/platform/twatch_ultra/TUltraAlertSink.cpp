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

#include "TUltraAlertSink.h"

#include <LilyGoLib.h>
#include <lvgl.h>

namespace layertime {
namespace twatch_ultra {

// Moved unchanged from ReconService::poll() in Phase 0 Step 4.
void TUltraAlertSink::raise(const Alert &)
{
    lv_display_trigger_activity(nullptr);
    instance.vibrator();
}

} // namespace twatch_ultra
} // namespace layertime
