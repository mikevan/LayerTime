// Unit tests for src/core/logic/ReconSelection, moved out of ReconService in
// Phase 0 Step 3c. test_recon still drives the same rules through
// ReconService; this suite pins the core functions on their own.

#include "check.h"

#include "core/logic/ReconSelection.h"

using namespace layertime;
using namespace layertime::recon;

namespace {
const ReconTarget kSingles[] = {
    ReconTarget::Deauth, ReconTarget::Pwnagotchi, ReconTarget::MultiSSID, ReconTarget::Flock,
    ReconTarget::Pineapple, ReconTarget::AirTag, ReconTarget::Flipper, ReconTarget::Meta,
    ReconTarget::Axon, ReconTarget::Tile, ReconTarget::SamsungTag, ReconTarget::GoogleTag};
}

void radio_per_single_detector()
{
    // BLE: Flock AirTag Flipper Meta Tile SamsungTag GoogleTag.
    // Wi-Fi: Deauth Pwnagotchi MultiSSID Pineapple Flock Axon.
    const bool ble[] = {false, false, false, true, false, true, true, true, false, true, true, true};
    const bool wifi[] = {true, true, true, true, true, false, false, false, true, false, false, false};
    for (size_t i = 0; i < 12; ++i) {
        CHECK_INT(ble[i], isBleDetector(kSingles[i]));
        CHECK_INT(wifi[i], isWifiDetector(kSingles[i]));
        CHECK_INT(ble[i], needsBle(kSingles[i]));
        CHECK_INT(wifi[i], needsWifi(kSingles[i]));
    }
}

void radio_per_group_and_all()
{
    CHECK_TRUE(needsBle(ReconTarget::All));
    CHECK_TRUE(needsWifi(ReconTarget::All));
    CHECK_TRUE(needsBle(ReconTarget::Trackers));
    CHECK_FALSE(needsWifi(ReconTarget::Trackers));
    CHECK_TRUE(needsBle(ReconTarget::CounterSurveil));
    CHECK_TRUE(needsWifi(ReconTarget::CounterSurveil));
    CHECK_TRUE(needsBle(ReconTarget::CounterIntrusion));   // Flipper
    CHECK_TRUE(needsWifi(ReconTarget::CounterIntrusion));
    CHECK_FALSE(needsBle(ReconTarget::None));
    CHECK_FALSE(needsWifi(ReconTarget::None));
    CHECK_FALSE(needsBle(ReconTarget::EarlyWarning));
    CHECK_FALSE(needsWifi(ReconTarget::EarlyWarning));
}

void group_contains_only_its_members()
{
    CHECK_TRUE(groupContains(ReconTarget::Trackers, ReconTarget::GoogleTag));
    CHECK_FALSE(groupContains(ReconTarget::Trackers, ReconTarget::Flock));
    CHECK_TRUE(groupContains(ReconTarget::CounterIntrusion, ReconTarget::Flipper));
    CHECK_FALSE(groupContains(ReconTarget::Deauth, ReconTarget::Deauth));  // not a group
    size_t n = 99;
    CHECK_TRUE(groupMembers(ReconTarget::All, n) == nullptr);
    CHECK_INT(0, n);
}

void ble_scan_scope()
{
    CHECK_TRUE(bleScanWants(ReconTarget::All, ReconTarget::Tile));
    CHECK_TRUE(bleScanWants(ReconTarget::EarlyWarning, ReconTarget::Flipper));
    CHECK_TRUE(bleScanWants(ReconTarget::EarlyWarning, ReconTarget::Meta));
    CHECK_FALSE(bleScanWants(ReconTarget::EarlyWarning, ReconTarget::AirTag));
    CHECK_TRUE(bleScanWants(ReconTarget::Tile, ReconTarget::Tile));
    CHECK_FALSE(bleScanWants(ReconTarget::Tile, ReconTarget::AirTag));
    CHECK_TRUE(bleScanWants(ReconTarget::Trackers, ReconTarget::AirTag));
    CHECK_FALSE(bleScanWants(ReconTarget::None, ReconTarget::AirTag));
}

void background_wifi_set()
{
    for (ReconTarget d : kSingles) {
        const bool expected = d == ReconTarget::Deauth || d == ReconTarget::Pwnagotchi ||
                              d == ReconTarget::Pineapple || d == ReconTarget::MultiSSID;
        CHECK_INT(expected, isBackgroundWifiDetector(d));
    }
}

void wants_during_manual_session()
{
    CHECK_TRUE(wants(true, ReconTarget::All, false, ReconTarget::Axon));
    CHECK_TRUE(wants(true, ReconTarget::Axon, false, ReconTarget::Axon));
    CHECK_FALSE(wants(true, ReconTarget::Axon, false, ReconTarget::Flock));
    CHECK_TRUE(wants(true, ReconTarget::CounterSurveil, false, ReconTarget::Flock));
    // A manual session ignores the sweep flag entirely.
    CHECK_FALSE(wants(true, ReconTarget::Axon, true, ReconTarget::Deauth));
}

void wants_outside_a_manual_session()
{
    CHECK_TRUE(wants(false, ReconTarget::None, true, ReconTarget::Pwnagotchi));
    CHECK_FALSE(wants(false, ReconTarget::None, true, ReconTarget::Flock));
    CHECK_FALSE(wants(false, ReconTarget::None, false, ReconTarget::Pwnagotchi));
    // The selection is ignored when not monitoring.
    CHECK_FALSE(wants(false, ReconTarget::All, false, ReconTarget::Axon));
}

void names()
{
    CHECK_STR("STOPPED", detectorName(ReconTarget::None));
    CHECK_STR("SMARTTAG", detectorName(ReconTarget::SamsungTag));
    CHECK_STR("COUNTER-SURVEIL", detectorName(ReconTarget::CounterSurveil));
    CHECK_STR("SURVEIL", detectorShortName(ReconTarget::CounterSurveil));
    CHECK_STR("EARLY WARN", detectorShortName(ReconTarget::EarlyWarning));
    CHECK_STR("FLOCK", detectorShortName(ReconTarget::Flock));
    CHECK_STR("MED", confidenceLabel(Confidence::Medium));
    CHECK_STR("LOW", confidenceLabel(Confidence::Low));
    CHECK_STR("HIGH", confidenceLabel(Confidence::High));
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(radio_per_single_detector);
    CASE(radio_per_group_and_all);
    CASE(group_contains_only_its_members);
    CASE(ble_scan_scope);
    CASE(background_wifi_set);
    CASE(wants_during_manual_session);
    CASE(wants_outside_a_manual_session);
    CASE(names);
    CHECK_SUMMARY();
}
