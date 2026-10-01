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

#include "S3PlusSettingsStore.h"

#include <Preferences.h>
#include <string.h>

#include "gnss/GnssRules.h"

namespace layertime {
namespace twatch_s3plus {

namespace {
constexpr const char *kAppNamespace = "layertime";
constexpr const char *kLocalNamespace = "lt_s3plus";
constexpr uint8_t kMeshCore = static_cast<uint8_t>(MeshNetwork::MeshCore);
constexpr uint8_t kMeshtastic = static_cast<uint8_t>(MeshNetwork::Meshtastic);
}

void S3PlusSettingsStore::load(ApplicationSettings &out)
{
    out = ApplicationSettings{};
    Preferences prefs;
    // Read-only open fails until the namespace has been written once; the
    // defaults then stand.
    if (!prefs.begin(kAppNamespace, true)) return;
    out.use24Hour = prefs.getBool("clock24", false);
    out.metricUnits = prefs.getBool("metric", false);
    out.meshAdvertising[kMeshCore] = prefs.getBool("meshadv", false);
    out.meshAdvertising[kMeshtastic] = prefs.getBool("mtadv", false);
    const String name = prefs.getString("mtname", "");
    strncpy(out.meshtasticName, name.c_str(), sizeof(out.meshtasticName) - 1);
    out.meshtasticName[sizeof(out.meshtasticName) - 1] = '\0';
    out.earlyWarningEnabled = prefs.getBool("reconew", true);
    out.sleepModeEnabled = prefs.getBool("sleepmode", false);
    prefs.end();
}

void S3PlusSettingsStore::save(const ApplicationSettings &settings)
{
    Preferences prefs;
    if (!prefs.begin(kAppNamespace, false)) return;
    prefs.putBool("clock24", settings.use24Hour);
    prefs.putBool("metric", settings.metricUnits);
    prefs.putBool("meshadv", settings.meshAdvertising[kMeshCore]);
    prefs.putBool("mtadv", settings.meshAdvertising[kMeshtastic]);
    prefs.putString("mtname", settings.meshtasticName);
    prefs.putBool("reconew", settings.earlyWarningEnabled);
    prefs.putBool("sleepmode", settings.sleepModeEnabled);
    prefs.end();
}

void S3PlusLocalSettings::load()
{
    Preferences prefs;
    if (!prefs.begin(kLocalNamespace, true)) return;
    timeZoneMinutes = gnss::clampOffset(prefs.getShort("tzmin", 0));
    const uint8_t b = prefs.getUChar("bright", kDefaultBrightness);
    brightness = b < kMinBrightness ? kMinBrightness : b;
    gpsEnabled = prefs.getBool("gps", true);
    dongleIcon = prefs.getBool("dongle", true);
    prefs.end();
}

void S3PlusLocalSettings::save() const
{
    Preferences prefs;
    if (!prefs.begin(kLocalNamespace, false)) return;
    prefs.putShort("tzmin", static_cast<int16_t>(gnss::clampOffset(timeZoneMinutes)));
    prefs.putUChar("bright", brightness);
    prefs.putBool("gps", gpsEnabled);
    prefs.putBool("dongle", dongleIcon);
    prefs.end();
}

} // namespace twatch_s3plus
} // namespace layertime
