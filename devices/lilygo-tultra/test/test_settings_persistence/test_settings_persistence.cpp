// Characterization tests for how LayerTime's settings are saved to and
// loaded from NVS. Written in Phase 0 Step 6a against devices/lilygo-tultra/src/services/
// SettingsService as it stood, before the settings split into application
// settings (owned by the core) and T-Ultra settings.
//
// What is pinned is what matters to a watch already in the field: the NVS
// namespace, every key name and value type, every default, the brightness
// floor, and that the two mesh radio switches are never stored. Settings
// already saved on a watch must load the same after any refactor.
//
// Preferences is the test-only in-memory stand-in in test/stubs/. Everything
// the tests need goes through the Harness, which presents one combined view
// of the settings, so that when the split lands only the Harness changes and
// every case stays as written.
//
// Since the split (Phase 0 Step 6), SettingsService saves the T-Ultra's own
// settings and TUltraSettingsStore saves the application settings, in the
// same namespace under the same keys. The Harness drives both.

#include "check.h"

#include <set>
#include <string>

#include <LilyGoLib.h>
#include <Preferences.h>

#include "TUltraSettingsStore.h"
#include "services/SettingsService.h"

namespace {

// Every setting, however it happens to be stored.
struct Settings {
    uint8_t brightness = 0;
    bool use24Hour = false;
    bool metricUnits = false;
    bool gpsEnabled = false;
    bool meshEnabled = false;
    bool meshAdvertiseEnabled = false;
    bool meshtasticEnabled = false;
    std::string meshtasticNodeName;
    bool meshtasticAdvertiseEnabled = false;
    bool reconEarlyWarningEnabled = false;
    bool reconSdLoggingEnabled = false;
    bool sleepModeEnabled = false;
    bool squachify = false;
};

// A starting point with every field away from its default, so a load that
// leaves a field alone is visible.
Settings allFlipped()
{
    Settings s;
    s.brightness = 200;
    s.use24Hour = true;
    s.metricUnits = true;
    s.gpsEnabled = false;
    s.meshEnabled = true;
    s.meshAdvertiseEnabled = true;
    s.meshtasticEnabled = true;
    s.meshtasticNodeName = "Ranger";
    s.meshtasticAdvertiseEnabled = true;
    s.reconEarlyWarningEnabled = false;
    s.reconSdLoggingEnabled = true;
    s.sleepModeEnabled = true;
    s.squachify = true;
    return s;
}

struct Harness {
    SettingsService service;
    layertime::twatch_ultra::TUltraSettingsStore store;

    Harness()
    {
        fake_nvs::reset();
        instance = FakeLilyGoInstance{};
    }

    static constexpr uint8_t kCore = static_cast<uint8_t>(layertime::MeshNetwork::MeshCore);
    static constexpr uint8_t kTastic = static_cast<uint8_t>(layertime::MeshNetwork::Meshtastic);

    static TUltraSettings toPlatform(const Settings &s)
    {
        TUltraSettings a;
        a.brightness = s.brightness;
        a.gpsEnabled = s.gpsEnabled;
        a.meshEnabled = s.meshEnabled;
        a.meshtasticEnabled = s.meshtasticEnabled;
        a.reconSdLoggingEnabled = s.reconSdLoggingEnabled;
        a.squachify = s.squachify;
        return a;
    }
    static layertime::ApplicationSettings toApplication(const Settings &s)
    {
        layertime::ApplicationSettings c;
        c.use24Hour = s.use24Hour;
        c.metricUnits = s.metricUnits;
        c.meshAdvertising[kCore] = s.meshAdvertiseEnabled;
        c.meshAdvertising[kTastic] = s.meshtasticAdvertiseEnabled;
        snprintf(c.meshtasticName, sizeof(c.meshtasticName), "%s", s.meshtasticNodeName.c_str());
        c.earlyWarningEnabled = s.reconEarlyWarningEnabled;
        c.sleepModeEnabled = s.sleepModeEnabled;
        return c;
    }
    static Settings combined(const TUltraSettings &a, const layertime::ApplicationSettings &c)
    {
        Settings s;
        s.brightness = a.brightness;
        s.gpsEnabled = a.gpsEnabled;
        s.meshEnabled = a.meshEnabled;
        s.meshtasticEnabled = a.meshtasticEnabled;
        s.reconSdLoggingEnabled = a.reconSdLoggingEnabled;
        s.squachify = a.squachify;
        s.use24Hour = c.use24Hour;
        s.metricUnits = c.metricUnits;
        s.meshAdvertiseEnabled = c.meshAdvertising[kCore];
        s.meshtasticAdvertiseEnabled = c.meshAdvertising[kTastic];
        s.meshtasticNodeName = c.meshtasticName;
        s.reconEarlyWarningEnabled = c.earlyWarningEnabled;
        s.sleepModeEnabled = c.sleepModeEnabled;
        return s;
    }

    // Loads into `start` and returns what the watch would then hold.
    Settings load(const Settings &start = Settings{})
    {
        TUltraSettings a = toPlatform(start);
        layertime::ApplicationSettings c = toApplication(start);
        service.load(a);
        store.load(c);
        return combined(a, c);
    }
    void save(const Settings &s)
    {
        service.save(toPlatform(s));
        store.save(toApplication(s));
    }
    void apply(const Settings &s) { service.apply(toPlatform(s)); }

    // ---- the store itself
    std::set<std::string> namespaces() const
    {
        std::set<std::string> out;
        for (const auto &ns : fake_nvs::g_store) out.insert(ns.first);
        return out;
    }
    std::string keys(const char *ns) const
    {
        std::string out;
        auto it = fake_nvs::g_store.find(ns);
        if (it == fake_nvs::g_store.end()) return out;
        for (const auto &kv : it->second) {
            const char *t = kv.second.type == fake_nvs::Type::UChar ? "u8"
                          : kv.second.type == fake_nvs::Type::Bool  ? "bool"
                                                                    : "str";
            out += (out.empty() ? "" : " ") + kv.first + ":" + t;
        }
        return out;
    }
    void storeUChar(const char *key, uint8_t v) { Preferences p; p.begin("layertime"); p.putUChar(key, v); p.end(); }
    void storeBool(const char *key, bool v) { Preferences p; p.begin("layertime"); p.putBool(key, v); p.end(); }
    void storeString(const char *key, const char *v) { Preferences p; p.begin("layertime"); p.putString(key, v); p.end(); }
    long storedNumber(const char *key) const { return fake_nvs::g_store["layertime"][key].number; }
    std::string storedText(const char *key) const { return fake_nvs::g_store["layertime"][key].text; }
};

void checkDefaults(const Settings &s)
{
    CHECK_INT(80, s.brightness);
    CHECK_FALSE(s.use24Hour);
    CHECK_FALSE(s.metricUnits);
    CHECK_TRUE(s.gpsEnabled);
    CHECK_FALSE(s.meshAdvertiseEnabled);
    CHECK_FALSE(s.meshtasticAdvertiseEnabled);
    CHECK_STR("", s.meshtasticNodeName.c_str());
    CHECK_TRUE(s.reconEarlyWarningEnabled);
    CHECK_FALSE(s.reconSdLoggingEnabled);
    CHECK_FALSE(s.sleepModeEnabled);
    CHECK_FALSE(s.squachify);
}

} // namespace

void a_watch_that_cannot_open_its_store_boots_on_the_defaults()
{
    Harness h;
    fake_nvs::g_available = false;
    const Settings s = h.load(allFlipped());
    checkDefaults(s);
}

void an_empty_store_gives_the_same_defaults()
{
    Harness h;
    const Settings s = h.load(allFlipped());
    checkDefaults(s);
}

void the_radio_switches_are_left_as_they_were_by_a_load()
{
    // They are never stored, so a load neither sets nor clears them. At boot
    // they start false because the settings start false.
    Harness h;
    Settings s = h.load(allFlipped());
    CHECK_TRUE(s.meshEnabled);
    CHECK_TRUE(s.meshtasticEnabled);
    fake_nvs::g_available = false;
    s = h.load(allFlipped());
    CHECK_TRUE(s.meshEnabled);
    CHECK_TRUE(s.meshtasticEnabled);
    s = h.load(Settings{});
    CHECK_FALSE(s.meshEnabled);
    CHECK_FALSE(s.meshtasticEnabled);
}

void save_writes_exactly_these_keys_in_one_namespace()
{
    Harness h;
    h.save(allFlipped());
    CHECK_INT(1, h.namespaces().size());
    CHECK_TRUE(h.namespaces().count("layertime") == 1);
    CHECK_STR("bright:u8 clock24:bool gps:bool meshadv:bool metric:bool mtadv:bool mtname:str "
              "reconew:bool reconsd:bool sleepmode:bool squach:bool",
              h.keys("layertime").c_str());
}

void every_stored_value_round_trips()
{
    Harness h;
    const Settings saved = allFlipped();
    h.save(saved);
    const Settings s = h.load(Settings{});
    CHECK_INT(200, s.brightness);
    CHECK_TRUE(s.use24Hour);
    CHECK_TRUE(s.metricUnits);
    CHECK_FALSE(s.gpsEnabled);
    CHECK_TRUE(s.meshAdvertiseEnabled);
    CHECK_STR("Ranger", s.meshtasticNodeName.c_str());
    CHECK_TRUE(s.meshtasticAdvertiseEnabled);
    CHECK_FALSE(s.reconEarlyWarningEnabled);
    CHECK_TRUE(s.reconSdLoggingEnabled);
    CHECK_TRUE(s.sleepModeEnabled);
    CHECK_TRUE(s.squachify);
    // Not stored, so not restored.
    CHECK_FALSE(s.meshEnabled);
    CHECK_FALSE(s.meshtasticEnabled);
}

void values_already_on_a_watch_load_by_their_key_names()
{
    Harness h;
    h.storeUChar("bright", 150);
    h.storeBool("clock24", true);
    h.storeBool("metric", true);
    h.storeBool("gps", false);
    h.storeBool("meshadv", true);
    h.storeBool("mtadv", true);
    h.storeString("mtname", "Hawk");
    h.storeBool("reconew", false);
    h.storeBool("reconsd", true);
    h.storeBool("sleepmode", true);
    h.storeBool("squach", true);
    const Settings s = h.load(Settings{});
    CHECK_INT(150, s.brightness);
    CHECK_TRUE(s.use24Hour);
    CHECK_TRUE(s.metricUnits);
    CHECK_FALSE(s.gpsEnabled);
    CHECK_TRUE(s.meshAdvertiseEnabled);
    CHECK_TRUE(s.meshtasticAdvertiseEnabled);
    CHECK_STR("Hawk", s.meshtasticNodeName.c_str());
    CHECK_FALSE(s.reconEarlyWarningEnabled);
    CHECK_TRUE(s.reconSdLoggingEnabled);
    CHECK_TRUE(s.sleepModeEnabled);
    CHECK_TRUE(s.squachify);
}

void stored_brightness_below_20_loads_as_20()
{
    Harness h;
    h.storeUChar("bright", 5);
    CHECK_INT(20, h.load().brightness);
    h.storeUChar("bright", 19);
    CHECK_INT(20, h.load().brightness);
    h.storeUChar("bright", 20);
    CHECK_INT(20, h.load().brightness);
    h.storeUChar("bright", 255);
    CHECK_INT(255, h.load().brightness);
    h.storeUChar("bright", 0);
    CHECK_INT(20, h.load().brightness);
}

void a_stored_name_longer_than_19_loads_cut_to_19()
{
    Harness h;
    h.storeString("mtname", "abcdefghijklmnopqrstuvwxyz");
    CHECK_STR("abcdefghijklmnopqrs", h.load().meshtasticNodeName.c_str());
}

void save_with_the_store_unavailable_writes_nothing()
{
    Harness h;
    fake_nvs::g_available = false;
    h.save(allFlipped());
    CHECK_INT(0, h.namespaces().size());
}

void save_overwrites_earlier_values()
{
    Harness h;
    h.save(allFlipped());
    Settings s = allFlipped();
    s.use24Hour = false;
    s.meshtasticNodeName = "";
    s.brightness = 90;
    h.save(s);
    CHECK_INT(0, h.storedNumber("clock24"));
    CHECK_STR("", h.storedText("mtname").c_str());
    CHECK_INT(90, h.storedNumber("bright"));
}

void apply_sets_the_backlight_to_the_brightness()
{
    Harness h;
    Settings s = allFlipped();
    s.brightness = 123;
    h.apply(s);
    CHECK_INT(123, instance.brightness);
}

// Added after the split in Phase 0 Step 6: which half owns which key. The
// two together write exactly the keys above.
void each_half_writes_only_its_own_keys()
{
    Harness h;
    h.service.save(Harness::toPlatform(allFlipped()));
    CHECK_STR("bright:u8 gps:bool reconsd:bool squach:bool", h.keys("layertime").c_str());
    fake_nvs::reset();
    h.store.save(Harness::toApplication(allFlipped()));
    CHECK_STR("clock24:bool meshadv:bool metric:bool mtadv:bool mtname:str reconew:bool sleepmode:bool",
              h.keys("layertime").c_str());
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(a_watch_that_cannot_open_its_store_boots_on_the_defaults);
    CASE(an_empty_store_gives_the_same_defaults);
    CASE(the_radio_switches_are_left_as_they_were_by_a_load);
    CASE(save_writes_exactly_these_keys_in_one_namespace);
    CASE(every_stored_value_round_trips);
    CASE(values_already_on_a_watch_load_by_their_key_names);
    CASE(stored_brightness_below_20_loads_as_20);
    CASE(a_stored_name_longer_than_19_loads_cut_to_19);
    CASE(save_with_the_store_unavailable_writes_nothing);
    CASE(save_overwrites_earlier_values);
    CASE(apply_sets_the_backlight_to_the_brightness);
    CASE(each_half_writes_only_its_own_keys);
    CHECK_SUMMARY();
}
