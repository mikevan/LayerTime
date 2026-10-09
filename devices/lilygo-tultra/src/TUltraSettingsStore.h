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

// SettingsStore on the T-Watch Ultra: NVS, through Preferences.
//
// Same namespace, keys, value types, and defaults SettingsService used for
// these settings before Phase 0 Step 6 split them out, so settings already
// saved on a watch load unchanged. SettingsService keeps the T-Ultra-only
// keys in the same namespace.

#include "core/ports/SettingsStore.h"

namespace layertime {
namespace twatch_ultra {

class TUltraSettingsStore : public SettingsStore {
public:
    void load(ApplicationSettings &out) override;
    void save(const ApplicationSettings &settings) override;
};

} // namespace twatch_ultra
} // namespace layertime
