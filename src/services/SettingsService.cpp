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

#include "SettingsService.h"

#include <Preferences.h>
#include <LilyGoLib.h>
#include <string.h>

namespace {
constexpr const char *kNamespace = "layertime";
constexpr uint8_t kDefaultBrightness = 80;
}

// The T-Ultra-only settings. Since Phase 0 Step 6 the application settings
// (clock format, units, sleep mode, early warning, mesh advertising and the
// Meshtastic name) are the core's, kept in this same NVS namespace by
// src/platform/twatch_ultra/TUltraSettingsStore under the same keys.

void SettingsService::load(AppSettings &settings)
{
    Preferences prefs;
    if (!prefs.begin(kNamespace, true)) {
        settings.brightness = kDefaultBrightness;
        settings.gpsEnabled = true;
        settings.reconSdLoggingEnabled = false;
        settings.squachify = false;
        // settings.meshEnabled/meshtasticEnabled intentionally left at their
        // struct defaults (false) here and below - they are never read from
        // Preferences, so radio power always starts off, every boot.
        return;
    }

    settings.brightness = prefs.getUChar("bright", kDefaultBrightness);
    settings.gpsEnabled = prefs.getBool("gps", true);
    settings.reconSdLoggingEnabled = prefs.getBool("reconsd", false);
    settings.squachify = prefs.getBool("squach", false);
    prefs.end();

    if (settings.brightness < 20) {
        settings.brightness = 20;
    }
}

void SettingsService::save(const AppSettings &settings)
{
    Preferences prefs;
    if (!prefs.begin(kNamespace, false)) {
        return;
    }

    prefs.putUChar("bright", settings.brightness);
    prefs.putBool("gps", settings.gpsEnabled);
    prefs.putBool("reconsd", settings.reconSdLoggingEnabled);
    prefs.putBool("squach", settings.squachify);
    // meshEnabled/meshtasticEnabled are deliberately never written here -
    // they must not persist.
    prefs.end();
}

void SettingsService::apply(const AppSettings &settings)
{
    instance.setBrightness(settings.brightness);
}
