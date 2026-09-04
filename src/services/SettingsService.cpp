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

void SettingsService::load(AppSettings &settings)
{
    Preferences prefs;
    if (!prefs.begin(kNamespace, true)) {
        settings.brightness = kDefaultBrightness;
        settings.use24Hour = false;
        settings.metricUnits = false;
        settings.gpsEnabled = true;
        settings.meshAdvertiseEnabled = false;
        settings.meshtasticAdvertiseEnabled = false;
        settings.meshtasticNodeName[0] = '\0';
        settings.reconEarlyWarningEnabled = true;
        settings.reconSdLoggingEnabled = false;
        settings.sleepModeEnabled = false;
        settings.squatchify = false;
        // settings.meshEnabled/meshtasticEnabled intentionally left at their
        // struct defaults (false) here and below - they are never read from
        // Preferences, so radio power always starts off, every boot.
        return;
    }

    settings.brightness = prefs.getUChar("bright", kDefaultBrightness);
    settings.use24Hour = prefs.getBool("clock24", false);
    settings.metricUnits = prefs.getBool("metric", false);
    settings.gpsEnabled = prefs.getBool("gps", true);
    settings.meshAdvertiseEnabled = prefs.getBool("meshadv", false);
    settings.meshtasticAdvertiseEnabled = prefs.getBool("mtadv", false);
    const String mtName = prefs.getString("mtname", "");
    strncpy(settings.meshtasticNodeName, mtName.c_str(), sizeof(settings.meshtasticNodeName) - 1);
    settings.meshtasticNodeName[sizeof(settings.meshtasticNodeName) - 1] = '\0';
    settings.reconEarlyWarningEnabled = prefs.getBool("reconew", true);
    settings.reconSdLoggingEnabled = prefs.getBool("reconsd", false);
    settings.sleepModeEnabled = prefs.getBool("sleepmode", false);
    settings.squatchify = prefs.getBool("squatch", false);
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
    prefs.putBool("clock24", settings.use24Hour);
    prefs.putBool("metric", settings.metricUnits);
    prefs.putBool("gps", settings.gpsEnabled);
    prefs.putBool("meshadv", settings.meshAdvertiseEnabled);
    prefs.putBool("mtadv", settings.meshtasticAdvertiseEnabled);
    prefs.putString("mtname", settings.meshtasticNodeName);
    prefs.putBool("reconew", settings.reconEarlyWarningEnabled);
    prefs.putBool("reconsd", settings.reconSdLoggingEnabled);
    prefs.putBool("sleepmode", settings.sleepModeEnabled);
    prefs.putBool("squatch", settings.squatchify);
    // meshEnabled/meshtasticEnabled are deliberately never written here -
    // they must not persist.
    prefs.end();
}

void SettingsService::apply(const AppSettings &settings)
{
    instance.setBrightness(settings.brightness);
}
