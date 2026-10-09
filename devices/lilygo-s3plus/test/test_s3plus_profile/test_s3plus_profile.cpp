// The S3 Plus capability binding (devices/lilygo-s3plus/src/S3PlusProfile.h) against
// its profile vector (contracts/vectors/profile_twatch_s3plus.json): every
// field's "effective" value must equal what the binding reports. The vector's
// own rules (status, evidence, the effective-value rule) are checked by the
// shared suite test/test_core_model, the same as for every other target.
//
// Run from devices/lilygo-s3plus/test/:
//   g++ -std=c++17 -O0 -Wall -Wextra -I../../../test -I../../../src -I../../../sensors/src -I../src -o tests_s3plus_profile test_s3plus_profile/test_s3plus_profile.cpp
//   ./tests_s3plus_profile

#include "check.h"

#include <fstream>
#include <regex>
#include <sstream>
#include <string>

#include "S3PlusProfile.h"

using layertime::DeviceCapabilities;
using layertime::DisplayClass;
using layertime::DisplayShape;

namespace {

std::string readVector()
{
    std::ifstream in("../../../contracts/vectors/profile_twatch_s3plus.json");
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// The text after "effective": inside the named capability's object,
// unquoted. Empty when the field is missing.
std::string effective(const std::string &json, const std::string &field)
{
    const std::regex re("\"" + field + "\"\\s*:\\s*\\{\\s*\"effective\"\\s*:\\s*(\"[^\"]*\"|[^,}\\s]+)");
    std::smatch m;
    if (!std::regex_search(json, m, re)) return "";
    std::string v = m[1].str();
    if (!v.empty() && v.front() == '"') v = v.substr(1, v.size() - 2);
    return v;
}

std::string b(bool v) { return v ? "true" : "false"; }

std::string bound(const DeviceCapabilities &c, const std::string &f)
{
    if (f == "displayClass") return c.displayClass == DisplayClass::ColorHighRes ? "ColorHighRes" : "MonochromeLowRes";
    if (f == "displayShape") return c.displayShape == DisplayShape::Round ? "Round" : "Rectangle";
    if (f == "displayWidth") return std::to_string(c.displayWidth);
    if (f == "displayHeight") return std::to_string(c.displayHeight);
    if (f == "touch") return b(c.touch);
    if (f == "buttons") return b(c.buttons);
    if (f == "vibration") return b(c.vibration);
    if (f == "gps") return b(c.gps);
    if (f == "compass") return b(c.compass);
    if (f == "altitude") return b(c.altitude);
    if (f == "nativeMaps") return b(c.nativeMaps);
    if (f == "removableStorage") return b(c.removableStorage);
    if (f == "localWifiMonitor") return b(c.localWifiMonitor);
    if (f == "localBleMonitor") return b(c.localBleMonitor);
    if (f == "externalRecon") return b(c.externalRecon);
    if (f == "meshUi") return b(c.meshUi);
    if (f == "localMeshtasticRadio") return b(c.localMeshtasticRadio);
    if (f == "localMeshCoreRadio") return b(c.localMeshCoreRadio);
    if (f == "meshRadioShared") return b(c.meshRadioShared);
    if (f == "androidBridge") return b(c.androidBridge);
    return "<unknown field>";
}

const char *const kFields[] = {
    "displayClass", "displayShape", "displayWidth", "displayHeight", "touch", "buttons",
    "vibration", "gps", "compass", "altitude", "nativeMaps", "removableStorage",
    "localWifiMonitor", "localBleMonitor", "externalRecon", "meshUi",
    "localMeshtasticRadio", "localMeshCoreRadio", "meshRadioShared", "androidBridge"};

} // namespace

void profile_id_matches_the_vector()
{
    const std::string j = readVector();
    CHECK_TRUE(!j.empty());
    CHECK_TRUE(j.find("\"profileId\": \"twatch-s3plus\"") != std::string::npos);
    CHECK_STR("twatch-s3plus", layertime::twatch_s3plus::capabilities().profileId);
}

void binding_matches_every_effective_value()
{
    const std::string j = readVector();
    const DeviceCapabilities c = layertime::twatch_s3plus::capabilities();
    for (const char *f : kFields) {
        const std::string eff = effective(j, f);
        const std::string bnd = bound(c, f);
        ++check::g_checks;
        if (eff.empty() || eff != bnd) {
            char msg[256];
            std::snprintf(msg, sizeof(msg), "%s: vector effective \"%s\" vs binding \"%s\"", f,
                          eff.c_str(), bnd.c_str());
            check::fail(__FILE__, __LINE__, msg);
        }
    }
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(profile_id_matches_the_vector);
    CASE(binding_matches_every_effective_value);
    CHECK_SUMMARY();
}
