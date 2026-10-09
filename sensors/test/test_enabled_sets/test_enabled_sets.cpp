// The enabled-detector set is the only way a caller chooses detectors. These
// tests prove, for both classifiers and every one of the 4,096 possible
// sets, that a set does exactly one thing: it removes the detectors outside
// it. Output for the detectors inside it is identical, in content and order,
// to running with every detector enabled. A detector outside the set also
// keeps no state, so switching it on later starts it from nothing.

#include "check.h"

#include <cstring>
#include <string>
#include <vector>

#include <lts/BleAdvertClassifier.h>
#include <lts/Signatures.h>
#include <lts/WifiFrameClassifier.h>

using namespace lts;

namespace {

struct Seen {
    DetectorId d;
    std::string detail, address;
    int rssi, conf, channel;
    bool operator==(const Seen &o) const
    {
        return d == o.d && detail == o.detail && address == o.address && rssi == o.rssi &&
               conf == o.conf && channel == o.channel;
    }
};

void collect(const Candidate &c, void *ctx)
{
    static_cast<std::vector<Seen> *>(ctx)->push_back(
        {c.detector, c.detail ? c.detail : "", c.address ? c.address : "", c.rssi,
         static_cast<int>(c.confidence), c.channel});
}

DetectorSet fromMask(uint32_t mask)
{
    DetectorSet s;
    for (size_t i = 0; i < kDetectorCount; ++i)
        if (mask & (1u << i)) s.add(kAllDetectors[i]);
    return s;
}

std::vector<Seen> only(const std::vector<Seen> &all, const DetectorSet &s)
{
    std::vector<Seen> out;
    for (const Seen &x : all)
        if (s.contains(x.d)) out.push_back(x);
    return out;
}

// ---- BLE ----

struct Advert {
    std::string name;
    uint8_t bytes[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
    std::vector<std::string> mfg;
    std::vector<uint16_t> uuids;
    static void mfgFn(uint8_t i, std::string &out, const void *c) { out = static_cast<const Advert *>(c)->mfg[i]; }
    static bool uuidFn(uint8_t i, uint16_t &out, const void *c)
    {
        out = static_cast<const Advert *>(c)->uuids[i];
        return true;
    }
};

std::vector<Seen> runBle(const std::vector<Advert> &corpus, const DetectorSet &s)
{
    std::vector<Seen> out;
    for (const Advert &a : corpus) {
        BleAdvertSource src;
        src.name = a.name.data();
        src.nameLength = a.name.size();
        src.printedAddress = "02:11:22:33:44:55";
        src.addressBytes = a.bytes;
        src.rssi = -50;
        src.manufacturerCount = static_cast<uint8_t>(a.mfg.size());
        src.manufacturer = Advert::mfgFn;
        src.uuidCount = static_cast<uint8_t>(a.uuids.size());
        src.uuid16 = Advert::uuidFn;
        src.context = &a;
        classifyBleAdvert(src, s, collect, &out);
    }
    return out;
}

std::vector<Advert> bleCorpus()
{
    std::vector<Advert> v;
    for (const BleUuidSignature &sig : kBleUuidSignatures) {
        Advert a;
        a.uuids = {sig.uuid};
        v.push_back(a);
    }
    Advert apple;
    apple.mfg = {std::string("\x4C\x00\x12\x19", 4)};
    v.push_back(apple);
    Advert flock;
    flock.name = "Penguin-7";
    flock.mfg = {std::string("\xC8\x09", 2)};
    memcpy(flock.bytes, "\x58\x8E\x81", 3);
    flock.uuids = {0xFEED, 0x3081};
    v.push_back(flock);
    Advert axon;
    memcpy(axon.bytes, "\x00\x25\xDF", 3);
    axon.uuids = {0xFD5F};
    v.push_back(axon);
    return v;
}

// ---- Wi-Fi ----

std::vector<uint8_t> mgmt(uint8_t subtype, const uint8_t *tx, size_t len)
{
    std::vector<uint8_t> f(len, 0);
    f[0] = static_cast<uint8_t>(subtype << 4);
    memcpy(&f[10], tx, 6);
    return f;
}

std::vector<uint8_t> beacon(const uint8_t *bssid, const char *ssid, bool privacy)
{
    const size_t n = strlen(ssid);
    std::vector<uint8_t> f = mgmt(0x08, bssid, 38 + n);
    f[34] = privacy ? 0x10 : 0;
    f[37] = static_cast<uint8_t>(n);
    memcpy(&f[38], ssid, n);
    return f;
}

struct Frame {
    std::vector<uint8_t> f;
    uint32_t t;
};

std::vector<Frame> wifiCorpus()
{
    std::vector<Frame> v;
    uint32_t t = 1000;
    const uint8_t tx[6] = {0x02, 1, 2, 3, 4, 5};
    for (int i = 0; i < 7; ++i) v.push_back({mgmt(0x0C, tx, 26), t += 100});
    const uint8_t pwn[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD};
    v.push_back({beacon(pwn, "p", false), t += 10});
    const uint8_t flock[6] = {0x58, 0x8E, 0x81, 1, 2, 3};
    v.push_back({beacon(flock, "AB3-12", true), t += 10});
    const uint8_t pine[6] = {0x00, 0x13, 0x37, 1, 2, 3};
    v.push_back({beacon(pine, "free", false), t += 10});
    const uint8_t multi[6] = {0x02, 9, 9, 9, 9, 9};
    for (int r = 0; r < 3; ++r) {
        v.push_back({beacon(multi, "One", true), t += 10});
        v.push_back({beacon(multi, "Two", true), t += 10});
    }
    return v;
}

std::vector<Seen> runWifi(const std::vector<Frame> &corpus, const DetectorSet &s)
{
    WifiFrameClassifier k;
    std::vector<Seen> out;
    for (const Frame &f : corpus)
        k.classify(f.f.data(), static_cast<uint16_t>(f.f.size()), -40, 6, f.t, s, collect, &out);
    return out;
}

} // namespace

void ble_every_set_keeps_exactly_its_own_detectors()
{
    const std::vector<Advert> corpus = bleCorpus();
    const std::vector<Seen> all = runBle(corpus, DetectorSet::all());
    CHECK_TRUE(all.size() >= 12);
    int bad = 0;
    for (uint32_t m = 0; m < (1u << kDetectorCount); ++m) {
        const DetectorSet s = fromMask(m);
        if (!(runBle(corpus, s) == only(all, s))) ++bad;
    }
    CHECK_INT(0, bad);
    CHECK_INT(0, runBle(corpus, DetectorSet::none()).size());
}

void wifi_every_set_keeps_exactly_its_own_detectors()
{
    const std::vector<Frame> corpus = wifiCorpus();
    const std::vector<Seen> all = runWifi(corpus, DetectorSet::all());
    // Deauth, Pwnagotchi, Pineapple on the Pwnagotchi BSSID (DE:AD:BE is on
    // the Pineapple list too), Flock (OUI), Axon (SSID), Pineapple, MultiSSID.
    CHECK_INT(7, all.size());
    int bad = 0;
    for (uint32_t m = 0; m < (1u << kDetectorCount); ++m) {
        const DetectorSet s = fromMask(m);
        if (!(runWifi(corpus, s) == only(all, s))) ++bad;
    }
    CHECK_INT(0, bad);
    CHECK_INT(0, runWifi(corpus, DetectorSet::none()).size());
}

void a_disabled_detector_keeps_no_state()
{
    WifiFrameClassifier k;
    std::vector<Seen> out;
    const uint8_t tx[6] = {0x02, 7, 7, 7, 7, 7};
    DetectorSet deauth;
    deauth.add(DetectorId::Deauth);
    uint32_t t = 1000;
    // Five deauth frames with Deauth off, then Deauth on: the burst must
    // start counting from zero, so five more frames still do not fire.
    for (int i = 0; i < 5; ++i) {
        const auto f = mgmt(0x0C, tx, 26);
        k.classify(f.data(), 26, -40, 1, t += 10, DetectorSet::none(), collect, &out);
    }
    for (int i = 0; i < 5; ++i) {
        const auto f = mgmt(0x0C, tx, 26);
        k.classify(f.data(), 26, -40, 1, t += 10, deauth, collect, &out);
    }
    CHECK_INT(0, out.size());
    const auto f = mgmt(0x0C, tx, 26);
    k.classify(f.data(), 26, -40, 1, t += 10, deauth, collect, &out);
    CHECK_INT(1, out.size());
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(ble_every_set_keeps_exactly_its_own_detectors);
    CASE(wifi_every_set_keeps_exactly_its_own_detectors);
    CASE(a_disabled_detector_keeps_no_state);
    CHECK_SUMMARY();
}
