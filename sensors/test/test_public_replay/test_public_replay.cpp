// Public replay: recorded or synthetic over-the-air observations, replayed
// in time order through the classifiers, checked at the sensor output and
// nowhere past it. What a consumer does next (de-duplicating repeats,
// keeping the strongest confidence, alerting) is the consumer's business and
// is tested in the consumer, not here.
//
// Each observation carries its own time, so nothing sleeps.

#include "check.h"

#include <cstring>
#include <string>
#include <vector>

#include <lts/BleAdvertClassifier.h>
#include <lts/WifiFrameClassifier.h>

using namespace lts;

namespace {

struct Out {
    DetectorId d;
    Confidence c;
    std::string detail, address;
    int channel;
};

void collect(const Candidate &c, void *ctx)
{
    static_cast<std::vector<Out> *>(ctx)->push_back(
        {c.detector, c.confidence, c.detail ? c.detail : "", c.address ? c.address : "", c.channel});
}

struct BleObservation {
    uint32_t atMs;
    std::string address;
    std::string name;
    std::vector<std::string> mfg;
    std::vector<int> uuids;  // -1: not a 16-bit UUID
};

std::vector<Out> replayBle(const std::vector<BleObservation> &obs, const DetectorSet &enabled)
{
    std::vector<Out> out;
    for (const BleObservation &o : obs) {
        struct Ctx {
            const BleObservation *o;
        } ctx{&o};
        uint8_t bytes[6] = {0x02, 0, 0, 0, 0, 0};
        BleAdvertSource s;
        s.name = o.name.data();
        s.nameLength = o.name.size();
        s.printedAddress = o.address.c_str();
        s.addressBytes = bytes;
        s.rssi = -55;
        s.manufacturerCount = static_cast<uint8_t>(o.mfg.size());
        s.manufacturer = [](uint8_t i, std::string &r, const void *c) {
            r = static_cast<const Ctx *>(c)->o->mfg[i];
        };
        s.uuidCount = static_cast<uint8_t>(o.uuids.size());
        s.uuid16 = [](uint8_t i, uint16_t &r, const void *c) {
            const int v = static_cast<const Ctx *>(c)->o->uuids[i];
            if (v < 0) return false;
            r = static_cast<uint16_t>(v);
            return true;
        };
        s.context = &ctx;
        classifyBleAdvert(s, enabled, collect, &out);
    }
    return out;
}

std::string b(std::initializer_list<uint8_t> v) { return std::string(v.begin(), v.end()); }

DetectorSet only(DetectorId d)
{
    DetectorSet s;
    s.add(d);
    return s;
}

} // namespace

void valid_tile_feed_is_reported_high()
{
    const auto out = replayBle({{1000, "AA:00:00:00:00:01", "", {}, {0xFEED}}}, DetectorSet::all());
    CHECK_INT(1, out.size());
    CHECK_INT(static_cast<int>(DetectorId::Tile), static_cast<int>(out[0].d));
    CHECK_INT(static_cast<int>(Confidence::High), static_cast<int>(out[0].c));
    CHECK_STR("Tile tracker", out[0].detail.c_str());
    CHECK_STR("AA:00:00:00:00:01", out[0].address.c_str());
    CHECK_INT(0, out[0].channel);
}

void valid_flipper_uuid_is_reported_medium()
{
    const auto out = replayBle({{1000, "AA:00:00:00:00:02", "", {}, {0x3081}}}, DetectorSet::all());
    CHECK_INT(1, out.size());
    CHECK_INT(static_cast<int>(DetectorId::Flipper), static_cast<int>(out[0].d));
    CHECK_INT(static_cast<int>(Confidence::Medium), static_cast<int>(out[0].c));
}

void apple_record_without_find_my_subtype_is_not_reported()
{
    CHECK_INT(0, replayBle({{1000, "A", "", {b({0x4C, 0x00})}, {}}}, DetectorSet::all()).size());
    CHECK_INT(0, replayBle({{1000, "A", "", {b({0x4C, 0x00, 0x10})}, {}}}, DetectorSet::all()).size());
}

void truncated_manufacturer_record_is_skipped_not_misread()
{
    const auto out = replayBle({{1000, "A", "", {b({0x4C}), b({0x4C, 0x00, 0x12})}, {}}},
                               DetectorSet::all());
    CHECK_INT(1, out.size());
    CHECK_INT(static_cast<int>(DetectorId::AirTag), static_cast<int>(out[0].d));
}

void every_sighting_is_reported_repeats_are_the_consumers_to_merge()
{
    const auto out = replayBle({{1000, "A", "", {}, {0xFEED}}, {2000, "A", "", {}, {0xFEED}},
                                {3000, "A", "", {}, {0xFEED}}},
                               DetectorSet::all());
    CHECK_INT(3, out.size());
}

void a_changed_name_changes_the_detail_not_the_detector()
{
    const auto out = replayBle({{1000, "A", "Tag one", {}, {0xFEED}}, {2000, "A", "Tag two", {}, {0xFEED}}},
                               DetectorSet::all());
    CHECK_INT(2, out.size());
    CHECK_STR("Tag one", out[0].detail.c_str());
    CHECK_STR("Tag two", out[1].detail.c_str());
    CHECK_INT(static_cast<int>(out[0].d), static_cast<int>(out[1].d));
}

void low_then_high_matches_are_each_reported_at_their_own_grade()
{
    const auto out = replayBle({{1000, "A", "", {}, {0xFEB7}}, {2000, "A", "", {}, {0xFD5F}}},
                               DetectorSet::all());
    CHECK_INT(2, out.size());
    CHECK_INT(static_cast<int>(Confidence::Low), static_cast<int>(out[0].c));
    CHECK_INT(static_cast<int>(Confidence::High), static_cast<int>(out[1].c));
}

void the_enabled_set_scopes_the_output()
{
    const std::vector<BleObservation> obs = {{1000, "A", "", {}, {0xFEED, 0xFD5F}}};
    CHECK_INT(2, replayBle(obs, DetectorSet::all()).size());
    const auto meta = replayBle(obs, only(DetectorId::Meta));
    CHECK_INT(1, meta.size());
    CHECK_INT(static_cast<int>(DetectorId::Meta), static_cast<int>(meta[0].d));
    CHECK_INT(0, replayBle(obs, DetectorSet::none()).size());
}

void wifi_deauth_burst_timeline()
{
    WifiFrameClassifier k;
    std::vector<Out> out;
    const uint8_t tx[6] = {0x02, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB};
    std::vector<uint8_t> f(26, 0);
    f[0] = 0x0C << 4;
    memcpy(&f[10], tx, 6);
    // Five frames: ordinary traffic. The sixth inside 3 s: a burst. More
    // inside the 15 s cooldown: not reported again. After it: reported.
    uint32_t t = 10000;
    for (int i = 0; i < 5; ++i) k.classify(f.data(), 26, -40, 11, t += 100, DetectorSet::all(), collect, &out);
    CHECK_INT(0, out.size());
    k.classify(f.data(), 26, -40, 11, t += 100, DetectorSet::all(), collect, &out);
    CHECK_INT(1, out.size());
    CHECK_INT(11, out[0].channel);
    CHECK_STR("Deauth flood", out[0].detail.c_str());
    CHECK_STR("02:AB:AB:AB:AB:AB", out[0].address.c_str());
    for (int i = 0; i < 10; ++i) k.classify(f.data(), 26, -40, 11, t += 100, DetectorSet::all(), collect, &out);
    CHECK_INT(1, out.size());
    t += 15000;
    for (int i = 0; i < 6; ++i) k.classify(f.data(), 26, -40, 11, t += 100, DetectorSet::all(), collect, &out);
    CHECK_INT(2, out.size());
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(valid_tile_feed_is_reported_high);
    CASE(valid_flipper_uuid_is_reported_medium);
    CASE(apple_record_without_find_my_subtype_is_not_reported);
    CASE(truncated_manufacturer_record_is_skipped_not_misread);
    CASE(every_sighting_is_reported_repeats_are_the_consumers_to_merge);
    CASE(a_changed_name_changes_the_detail_not_the_detector);
    CASE(low_then_high_matches_are_each_reported_at_their_own_grade);
    CASE(the_enabled_set_scopes_the_output);
    CASE(wifi_deauth_burst_timeline);
    CHECK_SUMMARY();
}
