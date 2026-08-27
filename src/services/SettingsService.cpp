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
    // meshEnabled/meshtasticEnabled are deliberately never written here -
    // they must not persist.
    prefs.end();
}

void SettingsService::apply(const AppSettings &settings)
{
    instance.setBrightness(settings.brightness);
}
