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

// Core's classification entry point (core/logic/ReconClassification): the
// explicit detector mapping between ReconTarget and lts::DetectorId, the
// translation of every Recon selection and wants() rule into the explicit
// detector set the sensor library takes, and the numbers LayerTime stores
// and transmits. Every one of the 256 possible byte values is tried, so a
// sparse or unknown value can never slip through as a detector.

#include "check.h"

#include <string.h>

#include <type_traits>
#include <vector>

#include "core/logic/ReconClassification.h"
#include "core/logic/ReconSelection.h"

using namespace layertime;
using namespace layertime::recon;

namespace {

bool isSingleDetector(int v) { return v >= 5 && v <= 16; }

} // namespace

void every_single_detector_maps_both_ways()
{
    int mapped = 0;
    for (int v = 0; v <= 255; ++v) {
        lts::DetectorId d = lts::DetectorId::Deauth;
        const bool ok = toSensorDetector(static_cast<ReconTarget>(v), d);
        CHECK_INT(isSingleDetector(v), ok);
        if (!ok) continue;
        ++mapped;
        CHECK_INT(v, static_cast<int>(fromSensorDetector(d)));
    }
    CHECK_INT(12, mapped);
    for (lts::DetectorId d : lts::kAllDetectors) {
        lts::DetectorId back = lts::DetectorId::Deauth;
        CHECK_TRUE(toSensorDetector(fromSensorDetector(d), back));
        CHECK_INT(static_cast<int>(d), static_cast<int>(back));
    }
}

void selections_and_unknown_numbers_are_never_detectors()
{
    lts::DetectorId d = lts::DetectorId::Deauth;
    for (ReconTarget t : {ReconTarget::None, ReconTarget::All, ReconTarget::Trackers,
                          ReconTarget::CounterSurveil, ReconTarget::CounterIntrusion,
                          ReconTarget::EarlyWarning})
        CHECK_FALSE(toSensorDetector(t, d));
    for (int v = 0; v <= 255; ++v) {
        if (lts::isKnownDetector(static_cast<uint8_t>(v))) continue;
        CHECK_INT(static_cast<int>(ReconTarget::None),
                  static_cast<int>(fromSensorDetector(static_cast<lts::DetectorId>(v))));
    }
}

void ble_scan_sets_equal_bleScanWants_for_every_selection()
{
    int bad = 0;
    for (int s = 0; s <= 255; ++s) {
        const lts::DetectorSet set = bleScanDetectors(static_cast<ReconTarget>(s));
        for (int t = 0; t <= 255; ++t) {
            lts::DetectorId d;
            const bool single = toSensorDetector(static_cast<ReconTarget>(t), d);
            if (!single) continue;
            if (set.contains(d) != bleScanWants(static_cast<ReconTarget>(s), static_cast<ReconTarget>(t))) ++bad;
        }
        // Nothing outside the twelve detectors is ever a member.
        for (int v = 0; v <= 255; ++v)
            if (!lts::isKnownDetector(static_cast<uint8_t>(v)) && set.contains(static_cast<lts::DetectorId>(v))) ++bad;
    }
    CHECK_INT(0, bad);
}

void ble_scan_sets_for_the_menu_entries()
{
    auto members = [](ReconTarget s) {
        std::vector<int> out;
        const lts::DetectorSet set = bleScanDetectors(s);
        for (lts::DetectorId d : lts::kAllDetectors)
            if (set.contains(d)) out.push_back(static_cast<int>(d));
        return out;
    };
    CHECK_TRUE(members(ReconTarget::None).empty());
    CHECK_INT(12, members(ReconTarget::All).size());
    CHECK_TRUE((members(ReconTarget::EarlyWarning) == std::vector<int>{11, 12}));       // Flipper, Meta
    CHECK_TRUE((members(ReconTarget::Trackers) == std::vector<int>{10, 14, 15, 16}));   // AirTag, Tile, SmartTag, Google
    CHECK_TRUE((members(ReconTarget::CounterSurveil) == std::vector<int>{8, 12, 13}));  // Flock, Meta, Axon
    CHECK_TRUE((members(ReconTarget::CounterIntrusion) == std::vector<int>{5, 6, 7, 9, 11}));
    for (int v = 5; v <= 16; ++v) CHECK_TRUE((members(static_cast<ReconTarget>(v)) == std::vector<int>{v}));
    for (int v : {17 + 1, 19, 20, 100, 255}) CHECK_TRUE(members(static_cast<ReconTarget>(v)).empty());
}

struct Mode {
    bool monitoring;
    ReconTarget selection;
    bool earlyWarning;
};

bool modeWants(ReconTarget d, const void *ctx)
{
    const Mode *m = static_cast<const Mode *>(ctx);
    return wants(m->monitoring, m->selection, m->earlyWarning, d);
}

void wifi_sets_equal_wants_for_every_mode()
{
    int bad = 0, nonEmpty = 0;
    for (int monitoring = 0; monitoring <= 1; ++monitoring)
        for (int s = 0; s <= 255; ++s)
            for (int ew = 0; ew <= 1; ++ew) {
                Mode m{monitoring != 0, static_cast<ReconTarget>(s), ew != 0};
                const lts::DetectorSet set = enabledDetectors(modeWants, &m);
                if (!set.empty()) ++nonEmpty;
                for (lts::DetectorId d : lts::kAllDetectors)
                    if (set.contains(d) != wants(m.monitoring, m.selection, m.earlyWarning, fromSensorDetector(d))) ++bad;
            }
    CHECK_INT(0, bad);
    CHECK_TRUE(nonEmpty > 0);
}

struct Custom {
    std::vector<int> yes;
};

bool customWants(ReconTarget d, const void *ctx)
{
    for (int v : static_cast<const Custom *>(ctx)->yes)
        if (v == static_cast<int>(d)) return true;
    return false;
}

void custom_wants_rules_become_exactly_their_sets()
{
    // Empty, one, a cross-radio pair, everything, and only non-detectors.
    const std::vector<std::vector<int>> rules = {
        {}, {10}, {10, 5}, {5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}, {0, 1, 2, 3, 4, 17, 200}};
    for (const auto &rule : rules) {
        Custom c{rule};
        const lts::DetectorSet set = enabledDetectors(customWants, &c);
        for (lts::DetectorId d : lts::kAllDetectors) {
            bool expected = false;
            for (int v : rule) expected = expected || v == static_cast<int>(d);
            CHECK_INT(expected, set.contains(d));
        }
    }
}

void the_numbers_layertime_stores_and_sends_do_not_move()
{
    // ReconTarget travels in the link's status and event records, the
    // ReconStart command, and the C5 stage log (contracts/link.md).
    const int expected[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17};
    const ReconTarget all[] = {
        ReconTarget::None, ReconTarget::All, ReconTarget::Trackers, ReconTarget::CounterSurveil,
        ReconTarget::CounterIntrusion, ReconTarget::Deauth, ReconTarget::Pwnagotchi,
        ReconTarget::MultiSSID, ReconTarget::Flock, ReconTarget::Pineapple, ReconTarget::AirTag,
        ReconTarget::Flipper, ReconTarget::Meta, ReconTarget::Axon, ReconTarget::Tile,
        ReconTarget::SamsungTag, ReconTarget::GoogleTag, ReconTarget::EarlyWarning};
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); ++i)
        CHECK_INT(expected[i], static_cast<int>(all[i]));
    // Confidence, SourceKind, and Band are the library's types, unchanged.
    CHECK_TRUE((std::is_same<Confidence, lts::Confidence>::value));
    CHECK_TRUE((std::is_same<SourceKind, lts::SourceKind>::value));
    CHECK_TRUE((std::is_same<Band, lts::Band>::value));
    CHECK_INT(1, sizeof(ReconTarget));
    CHECK_INT(1, sizeof(Confidence));
    CHECK_INT(1, sizeof(SourceKind));
    CHECK_INT(1, sizeof(Band));
}

namespace {
std::vector<Candidate> g_seen;
void keep(const Candidate &c, void *) { g_seen.push_back(c); }
} // namespace

void core_candidates_carry_core_numbers()
{
    // A Meta UUID and a Tile UUID in one advert, scanned for All.
    struct Ad {
        static bool uuid(uint8_t i, uint16_t &out, const void *) { out = i == 0 ? 0xFD5F : 0xFEED; return true; }
    };
    const uint8_t address[6] = {0x02, 1, 2, 3, 4, 5};
    BleAdvertSource src;
    src.printedAddress = "02:01:02:03:04:05";
    src.addressBytes = address;
    src.rssi = -61;
    src.uuidCount = 2;
    src.uuid16 = Ad::uuid;
    g_seen.clear();
    classifyBleAdvert(src, ReconTarget::All, keep, nullptr);
    CHECK_INT(2, g_seen.size());
    CHECK_INT(static_cast<int>(ReconTarget::Meta), static_cast<int>(g_seen[0].detector));
    CHECK_INT(static_cast<int>(ReconTarget::Tile), static_cast<int>(g_seen[1].detector));
    CHECK_INT(2, static_cast<int>(g_seen[0].confidence));
    CHECK_INT(-61, g_seen[0].rssi);
    // The device stamps these; the classifiers never do.
    CHECK_INT(0, static_cast<int>(g_seen[0].sourceKind));
    CHECK_INT(0, static_cast<int>(g_seen[0].band));
    CHECK_INT(0, g_seen[0].atMs);
    g_seen.clear();
    classifyBleAdvert(src, ReconTarget::Tile, keep, nullptr);
    CHECK_INT(1, g_seen.size());
    g_seen.clear();
    classifyBleAdvert(src, ReconTarget::None, keep, nullptr);
    CHECK_INT(0, g_seen.size());
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(every_single_detector_maps_both_ways);
    CASE(selections_and_unknown_numbers_are_never_detectors);
    CASE(ble_scan_sets_equal_bleScanWants_for_every_selection);
    CASE(ble_scan_sets_for_the_menu_entries);
    CASE(wifi_sets_equal_wants_for_every_mode);
    CASE(custom_wants_rules_become_exactly_their_sets);
    CASE(the_numbers_layertime_stores_and_sends_do_not_move);
    CASE(core_candidates_carry_core_numbers);
    CHECK_SUMMARY();
}
