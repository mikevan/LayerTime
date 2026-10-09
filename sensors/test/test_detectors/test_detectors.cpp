// Unit tests for lts/Detectors.h: the stable numbers, the sparse detector
// list, and DetectorSet. Consumers store and transmit these numbers, so a
// change to any of them must fail here first.

#include "check.h"

#include <initializer_list>

#include <lts/Detectors.h>

using namespace lts;

void detector_numbers_are_stable()
{
    CHECK_INT(5, static_cast<int>(DetectorId::Deauth));
    CHECK_INT(6, static_cast<int>(DetectorId::Pwnagotchi));
    CHECK_INT(7, static_cast<int>(DetectorId::MultiSSID));
    CHECK_INT(8, static_cast<int>(DetectorId::Flock));
    CHECK_INT(9, static_cast<int>(DetectorId::Pineapple));
    CHECK_INT(10, static_cast<int>(DetectorId::AirTag));
    CHECK_INT(11, static_cast<int>(DetectorId::Flipper));
    CHECK_INT(12, static_cast<int>(DetectorId::Meta));
    CHECK_INT(13, static_cast<int>(DetectorId::Axon));
    CHECK_INT(14, static_cast<int>(DetectorId::Tile));
    CHECK_INT(15, static_cast<int>(DetectorId::SamsungTag));
    CHECK_INT(16, static_cast<int>(DetectorId::GoogleTag));
}

void confidence_source_and_band_numbers_are_stable()
{
    CHECK_INT(0, static_cast<int>(Confidence::Low));
    CHECK_INT(1, static_cast<int>(Confidence::Medium));
    CHECK_INT(2, static_cast<int>(Confidence::High));
    CHECK_TRUE(Confidence::Low < Confidence::Medium && Confidence::Medium < Confidence::High);
    CHECK_INT(0, static_cast<int>(SourceKind::Unknown));
    CHECK_INT(1, static_cast<int>(SourceKind::Wifi));
    CHECK_INT(2, static_cast<int>(SourceKind::Ble));
    CHECK_INT(3, static_cast<int>(SourceKind::Ieee802154));
    CHECK_INT(0, static_cast<int>(Band::Unknown));
    CHECK_INT(1, static_cast<int>(Band::Band2_4GHz));
    CHECK_INT(2, static_cast<int>(Band::Band5GHz));
}

void the_detector_list_is_sparse_unique_and_complete()
{
    CHECK_INT(12, kDetectorCount);
    int known = 0;
    for (int v = 0; v <= 255; ++v) {
        const bool listed = [&] {
            for (DetectorId d : kAllDetectors)
                if (static_cast<int>(d) == v) return true;
            return false;
        }();
        CHECK_INT(listed, isKnownDetector(static_cast<uint8_t>(v)));
        if (listed) ++known;
    }
    CHECK_INT(12, known);
    for (int v : {0, 1, 2, 3, 4, 17, 31, 32, 255}) CHECK_FALSE(isKnownDetector(static_cast<uint8_t>(v)));
}

void empty_and_full_sets()
{
    CHECK_TRUE(DetectorSet::none().empty());
    CHECK_TRUE(DetectorSet().empty());
    CHECK_FALSE(DetectorSet::all().empty());
    for (DetectorId d : kAllDetectors) {
        CHECK_FALSE(DetectorSet::none().contains(d));
        CHECK_TRUE(DetectorSet::all().contains(d));
    }
}

void unknown_numbers_never_become_members()
{
    for (int v = 0; v <= 255; ++v) {
        if (isKnownDetector(static_cast<uint8_t>(v))) continue;
        DetectorSet s;
        s.add(static_cast<DetectorId>(v));
        CHECK_TRUE(s.empty());
        CHECK_FALSE(DetectorSet::all().contains(static_cast<DetectorId>(v)));
        s.remove(static_cast<DetectorId>(v));  // must be harmless, even above 31
        CHECK_TRUE(s.empty());
    }
}

void add_remove_and_custom_combinations()
{
    DetectorSet s;
    s.add(DetectorId::AirTag);
    s.add(DetectorId::Deauth);
    s.add(DetectorId::AirTag);
    CHECK_TRUE(s.contains(DetectorId::AirTag));
    CHECK_TRUE(s.contains(DetectorId::Deauth));
    CHECK_FALSE(s.contains(DetectorId::Tile));
    s.remove(DetectorId::AirTag);
    CHECK_FALSE(s.contains(DetectorId::AirTag));
    CHECK_TRUE(s.contains(DetectorId::Deauth));
    DetectorSet t;
    t.add(DetectorId::Deauth);
    CHECK_TRUE(s == t);
    t.add(DetectorId::GoogleTag);
    CHECK_TRUE(s != t);
    // Every one of the 4096 combinations holds exactly its members.
    for (uint32_t mask = 0; mask < (1u << kDetectorCount); ++mask) {
        DetectorSet c;
        for (size_t i = 0; i < kDetectorCount; ++i)
            if (mask & (1u << i)) c.add(kAllDetectors[i]);
        for (size_t i = 0; i < kDetectorCount; ++i)
            if (c.contains(kAllDetectors[i]) != ((mask & (1u << i)) != 0)) CHECK_TRUE(false);
        if (c.empty() != (mask == 0)) CHECK_TRUE(false);
    }
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(detector_numbers_are_stable);
    CASE(confidence_source_and_band_numbers_are_stable);
    CASE(the_detector_list_is_sparse_unique_and_complete);
    CASE(empty_and_full_sets);
    CASE(unknown_numbers_never_become_members);
    CASE(add_remove_and_custom_combinations);
    CHECK_SUMMARY();
}
