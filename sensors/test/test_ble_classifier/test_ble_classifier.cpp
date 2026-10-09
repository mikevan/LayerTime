// Unit tests for lts/BleAdvertClassifier. Calls the classifier directly,
// with no stubs and no BLE stack.

#include "check.h"

#include <string>
#include <vector>

#include <lts/BleAdvertClassifier.h>

using namespace lts;

namespace {

struct Seen { DetectorId d; std::string detail, address; int rssi; Confidence c; int channel; };

struct Advert {
    std::string name;
    std::string printed = "aa:bb:cc:dd:ee:ff";
    uint8_t bytes[6] = {0x02, 0, 0, 0, 0, 0};
    int8_t rssi = -60;
    std::vector<std::string> mfg;
    std::vector<int> uuids;  // -1 = not a 16-bit UUID

    static void mfgFn(uint8_t i, std::string &out, const void *ctx)
    {
        out = static_cast<const Advert *>(ctx)->mfg[i];
    }
    static bool uuidFn(uint8_t i, uint16_t &out, const void *ctx)
    {
        const int v = static_cast<const Advert *>(ctx)->uuids[i];
        if (v < 0) return false;
        out = static_cast<uint16_t>(v);
        return true;
    }
    BleAdvertSource source() const
    {
        BleAdvertSource s;
        s.name = name.data();
        s.nameLength = name.size();
        s.printedAddress = printed.c_str();
        s.addressBytes = bytes;
        s.rssi = rssi;
        s.manufacturerCount = static_cast<uint8_t>(mfg.size());
        s.manufacturer = mfgFn;
        s.uuidCount = static_cast<uint8_t>(uuids.size());
        s.uuid16 = uuidFn;
        s.context = this;
        return s;
    }
};

std::vector<Seen> classify(const Advert &a, const DetectorSet &enabled)
{
    std::vector<Seen> out;
    classifyBleAdvert(a.source(), enabled,
                      [](const Candidate &c, void *ctx) {
                          static_cast<std::vector<Seen> *>(ctx)->push_back(
                              {c.detector, c.detail, c.address, c.rssi, c.confidence, c.channel});
                      },
                      &out);
    return out;
}

std::string bytes(std::initializer_list<uint8_t> b) { return std::string(b.begin(), b.end()); }

DetectorSet only(DetectorId d)
{
    DetectorSet s;
    s.add(d);
    return s;
}

} // namespace

void order_is_manufacturer_then_address_then_uuids()
{
    Advert a;
    a.name = "Penguin-1";
    a.uuids = {0xFEED};
    a.mfg = {bytes({0xC8, 0x09})};
    a.bytes[0] = 0x58; a.bytes[1] = 0x8E; a.bytes[2] = 0x81;  // Flock OUI in bytes 0..2
    const auto s = classify(a, DetectorSet::all());
    CHECK_INT(3, s.size());
    if (s.size() != 3) return;
    CHECK_INT(static_cast<int>(DetectorId::Flock), static_cast<int>(s[0].d));   // XUNTONG + name
    CHECK_INT(static_cast<int>(DetectorId::Flock), static_cast<int>(s[1].d));   // OUI
    CHECK_INT(static_cast<int>(DetectorId::Tile), static_cast<int>(s[2].d));    // UUID
    for (const Seen &x : s) {
        CHECK_STR("Penguin-1", x.detail.c_str());
        CHECK_STR("aa:bb:cc:dd:ee:ff", x.address.c_str());
        CHECK_INT(-60, x.rssi);
        CHECK_INT(0, x.channel);
    }
}

void oui_reads_bytes_zero_to_two_of_what_it_is_given()
{
    Advert a;
    a.bytes[3] = 0x58; a.bytes[4] = 0x8E; a.bytes[5] = 0x81;
    CHECK_INT(0, classify(a, DetectorSet::all()).size());
}

void short_manufacturer_records_are_skipped()
{
    Advert a;
    a.mfg = {bytes({0x4C}), bytes({0x4C, 0x00, 0x12})};
    const auto s = classify(a, only(DetectorId::AirTag));
    CHECK_INT(1, s.size());
}

void apple_record_too_short_for_a_subtype_is_ignored()
{
    Advert a;
    a.mfg = {bytes({0x4C, 0x00})};
    CHECK_INT(0, classify(a, only(DetectorId::AirTag)).size());
}

void unmatched_or_long_uuids_are_skipped_without_stopping()
{
    Advert a;
    a.uuids = {-1, 0x1234, 0xFEEC, 0xFD5A};
    const auto s = classify(a, DetectorSet::all());
    CHECK_INT(2, s.size());
    if (s.size() == 2) {
        CHECK_STR("Tile tracker", s[0].detail.c_str());
        CHECK_STR("Samsung SmartTag", s[1].detail.c_str());
    }
}

void a_uuid_outside_the_scan_is_dropped_not_retried()
{
    Advert a;
    a.uuids = {0xFD5F};  // Meta, High
    CHECK_INT(0, classify(a, only(DetectorId::Tile)).size());
    const auto s = classify(a, only(DetectorId::Meta));
    CHECK_INT(1, s.size());
    if (!s.empty())
        CHECK_INT(static_cast<int>(Confidence::High), static_cast<int>(s[0].c));
}

void name_length_is_honoured_even_with_an_embedded_nul()
{
    // "FS Ext Battery" followed by a NUL is 15 bytes, so it is not the exact
    // Flock battery name. The detail text still stops at the NUL.
    Advert a;
    a.name = std::string("FS Ext Battery\0", 15);
    a.mfg = {bytes({0xC8, 0x09})};
    CHECK_INT(0, classify(a, only(DetectorId::Flock)).size());
    a.name = std::string("Penguin-\0x", 10);
    const auto s = classify(a, only(DetectorId::Flock));
    CHECK_INT(1, s.size());
    if (!s.empty()) CHECK_STR("Penguin-", s[0].detail.c_str());
}

void no_name_falls_back_to_signature_labels()
{
    Advert a;
    a.mfg = {bytes({0xC8, 0x09})};
    a.bytes[0] = 0x00; a.bytes[1] = 0x25; a.bytes[2] = 0xDF;  // Axon OUI
    const auto s = classify(a, DetectorSet::all());
    CHECK_INT(2, s.size());
    if (s.size() == 2) {
        CHECK_STR("Flock BLE signature", s[0].detail.c_str());
        CHECK_STR("Axon (Taser)", s[1].detail.c_str());
    }
}

void nothing_is_reported_for_an_empty_scan()
{
    Advert a;
    a.uuids = {0xFEED};
    a.mfg = {bytes({0x4C, 0x00, 0x12})};
    CHECK_INT(0, classify(a, DetectorSet::none()).size());
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(order_is_manufacturer_then_address_then_uuids);
    CASE(oui_reads_bytes_zero_to_two_of_what_it_is_given);
    CASE(short_manufacturer_records_are_skipped);
    CASE(apple_record_too_short_for_a_subtype_is_ignored);
    CASE(unmatched_or_long_uuids_are_skipped_without_stopping);
    CASE(a_uuid_outside_the_scan_is_dropped_not_retried);
    CASE(name_length_is_honoured_even_with_an_embedded_nul);
    CASE(no_name_falls_back_to_signature_labels);
    CASE(nothing_is_reported_for_an_empty_scan);
    CHECK_SUMMARY();
}
