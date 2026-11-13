// Unit tests for libraries/LayerTime-Core/src/core/logic/ReconSelection, moved out of ReconService in
// Phase 0 Step 3c. test_recon still drives the same rules through
// ReconService; this suite pins the core functions on their own.

#include "check.h"

#include "core/logic/ReconSelection.h"

#include <initializer_list>

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

// SetSensors (2026-11-04): one bit per single sensor at its ReconTarget value.
void sensor_bits_cover_every_single_sensor_and_no_selection()
{
    CHECK_INT(0x0005FFE0u, kAllSensorsMask); // bits 5 to 16, and 18
    for (ReconTarget d : kSingles) CHECK_INT(1u << static_cast<uint8_t>(d), sensorBit(d));
    CHECK_INT(1u << 18, sensorBit(ReconTarget::Eapol));
    for (ReconTarget s : {ReconTarget::None, ReconTarget::All, ReconTarget::Trackers, ReconTarget::CounterSurveil,
                          ReconTarget::CounterIntrusion, ReconTarget::EarlyWarning})
        CHECK_INT(0, sensorBit(s));
    CHECK_INT(0, sensorBit(static_cast<ReconTarget>(200))); // past 31: no shift, no bit
}

void sensor_allowed_follows_the_mask()
{
    const uint32_t noTrackers = kAllSensorsMask & ~(sensorBit(ReconTarget::AirTag) | sensorBit(ReconTarget::Tile) |
                                                    sensorBit(ReconTarget::SamsungTag) | sensorBit(ReconTarget::GoogleTag));
    CHECK_FALSE(sensorAllowed(noTrackers, ReconTarget::AirTag));
    CHECK_FALSE(sensorAllowed(noTrackers, ReconTarget::GoogleTag));
    CHECK_TRUE(sensorAllowed(noTrackers, ReconTarget::Deauth));
    CHECK_TRUE(sensorAllowed(noTrackers, ReconTarget::Eapol));
    CHECK_FALSE(sensorAllowed(0, ReconTarget::Deauth));
    for (ReconTarget d : kSingles) CHECK_TRUE(sensorAllowed(kAllSensorsMask, d));
    // A selection never reports, whatever the mask says.
    CHECK_FALSE(sensorAllowed(0xFFFFFFFFu, ReconTarget::All));
}

void valid_masks_hold_only_sensor_bits()
{
    CHECK_TRUE(validSensorMask(0));
    CHECK_TRUE(validSensorMask(kAllSensorsMask));
    CHECK_TRUE(validSensorMask(sensorBit(ReconTarget::Eapol)));
    CHECK_FALSE(validSensorMask(1u << 1));  // All
    CHECK_FALSE(validSensorMask(1u << 2));  // a group
    CHECK_FALSE(validSensorMask(1u << 17)); // EarlyWarning
    CHECK_FALSE(validSensorMask(1u << 19));
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
    CASE(sensor_bits_cover_every_single_sensor_and_no_selection);
    CASE(sensor_allowed_follows_the_mask);
    CASE(valid_masks_hold_only_sensor_bits);
    CHECK_SUMMARY();
}
