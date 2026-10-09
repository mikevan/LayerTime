// Unit tests for lts/Signatures. Built with no stubs at all: the signature
// code needs nothing from any platform.

#include "check.h"

#include <set>
#include <string>

#include <lts/Signatures.h>

using namespace lts;

void company_ids_and_proximity_pairing_switch()
{
    CHECK_INT(0x004C, kAppleCompanyId);
    CHECK_INT(0x09C8, kXuntongCompanyId);
    CHECK_FALSE(kMatchProximityPairing);
}

void oui_table_has_no_duplicate_prefixes()
{
    std::set<uint32_t> seen;
    for (const OuiSignature &s : kOuiSignatures) {
        const uint32_t oui = (uint32_t(s.oui[0]) << 16) | (uint32_t(s.oui[1]) << 8) | s.oui[2];
        CHECK_TRUE(seen.insert(oui).second);
    }
    CHECK_INT(39, seen.size());
}

void oui_confidence_split_is_20_flock_high_3_axon_high_16_flock_low()
{
    int flockHigh = 0, axonHigh = 0, flockLow = 0, other = 0;
    for (const OuiSignature &s : kOuiSignatures) {
        if (s.detector == DetectorId::Flock && s.confidence == Confidence::High) ++flockHigh;
        else if (s.detector == DetectorId::Axon && s.confidence == Confidence::High) ++axonHigh;
        else if (s.detector == DetectorId::Flock && s.confidence == Confidence::Low) ++flockLow;
        else ++other;
    }
    CHECK_INT(20, flockHigh);
    CHECK_INT(3, axonHigh);
    CHECK_INT(16, flockLow);
    CHECK_INT(0, other);
}

void lookup_reads_the_first_three_bytes_given()
{
    const uint8_t mac[6] = {0x00, 0x25, 0xDF, 0x99, 0x99, 0x99};
    const OuiSignature *s = lookupOui(mac);
    CHECK_TRUE(s != nullptr);
    if (s) CHECK_STR("Axon (Taser)", s->label);
    const uint8_t reversed[6] = {0x99, 0x99, 0x99, 0xDF, 0x25, 0x00};
    CHECK_TRUE(lookupOui(reversed) == nullptr);
}

void ssid_prefixes_are_the_four_axon_prefixes()
{
    const char *expected[] = {"AB2-", "AB3-", "AB4-", "AXON-"};
    CHECK_INT(4, sizeof(kSsidPrefixes) / sizeof(kSsidPrefixes[0]));
    for (size_t i = 0; i < 4; ++i) {
        CHECK_STR(expected[i], kSsidPrefixes[i].prefix);
        CHECK_INT(static_cast<int>(DetectorId::Axon), static_cast<int>(kSsidPrefixes[i].detector));
        CHECK_INT(static_cast<int>(Confidence::High), static_cast<int>(kSsidPrefixes[i].confidence));
    }
}

void ssid_prefix_handles_non_letters_and_empty()
{
    const uint8_t s[] = {'1', '-', 'x'};
    CHECK_TRUE(ssidHasPrefix(s, 3, "1-"));
    CHECK_TRUE(ssidHasPrefix(s, 0, ""));
    CHECK_FALSE(ssidHasPrefix(s, 0, "1"));
}

void find_my_needs_exactly_three_bytes_minimum()
{
    const uint8_t three[] = {0x4C, 0x00, 0x12};
    CHECK_TRUE(isFindMyBeacon(three, 3));
    CHECK_FALSE(isFindMyBeacon(three, 0));
}

void mac_text_at_the_minimum_buffer()
{
    char out[18];
    const uint8_t mac[] = {0, 1, 2, 3, 4, 255};
    formatMac(out, sizeof(out), mac);
    CHECK_STR("00:01:02:03:04:FF", out);
    char keep[18] = "keep";
    formatMac(keep, sizeof(keep), nullptr);
    CHECK_STR("keep", keep);
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(company_ids_and_proximity_pairing_switch);
    CASE(oui_table_has_no_duplicate_prefixes);
    CASE(oui_confidence_split_is_20_flock_high_3_axon_high_16_flock_low);
    CASE(lookup_reads_the_first_three_bytes_given);
    CASE(ssid_prefixes_are_the_four_axon_prefixes);
    CASE(ssid_prefix_handles_non_letters_and_empty);
    CASE(find_my_needs_exactly_three_bytes_minimum);
    CASE(mac_text_at_the_minimum_buffer);
    CHECK_SUMMARY();
}
