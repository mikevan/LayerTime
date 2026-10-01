// LayerTime - counter-intrusion and resilient-communications firmware
// for the LilyGo T-Watch S3 Plus.
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

// Settings on the S3 Plus, in NVS through Preferences.
//
//   * The core's ApplicationSettings, in namespace "layertime" with the same
//     keys and defaults as the T-Ultra's TUltraSettingsStore (early warning
//     defaults to on). Only this watch's own NVS is ever touched.
//   * The S3 Plus's own settings, in namespace "lt_s3plus" (the bring-up's
//     namespace): the time-zone offset in minutes ("tzmin"), the display
//     brightness ("bright"), and whether the GNSS receiver is on ("gps",
//     default on; the Ultra keeps its GPS switch in its own platform
//     settings too), and whether the face shows the DONGLE icon ("dongle",
//     default on). These exist only because of this watch's
//     hardware or its clock, so they are platform settings, not the core's.

#include <stdint.h>

#include "core/ports/SettingsStore.h"

namespace layertime {
namespace twatch_s3plus {

class S3PlusSettingsStore : public SettingsStore {
public:
    void load(ApplicationSettings &out) override;
    void save(const ApplicationSettings &settings) override;
};

struct S3PlusLocalSettings {
    static constexpr uint8_t kDefaultBrightness = 200;
    // The dimmest the display goes: below it the screen is unreadable (same
    // floor idea as the Ultra's).
    static constexpr uint8_t kMinBrightness = 20;
    int timeZoneMinutes = 0;
    uint8_t brightness = kDefaultBrightness;
    bool gpsEnabled = true;
    bool dongleIcon = true;

    void load();
    void save() const;
};

} // namespace twatch_s3plus
} // namespace layertime
