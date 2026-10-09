// LayerTime deterministic sensor replay harness - BLE detection pipeline.
//
// Drives the REAL production code: core/logic/BleAdvertClassifier for
// classification and core/logic/MonitorEventLog (+ AlertPolicy) for the
// stateful history. No detection logic is reimplemented here. This file only:
//   1. records an observation (the bytes + metadata one receiver saw),
//   2. adapts it to the production BleAdvertSource through the same two
//      callbacks the device's NimBLE adapter uses,
//   3. routes every candidate the classifier emits into that receiver's own
//      MonitorEventLog, stamping atMs/band/source exactly as the platform does,
//   4. lets a host test read the resulting records back.
//
// Time is explicit (Observation::atMs); nothing sleeps. State is per receiver,
// so four wands never share one detection history.
#pragma once

#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "core/logic/BleAdvertClassifier.h"
#include "core/logic/MonitorEventLog.h"

namespace layertime {
namespace replay {

// One recorded or synthetic BLE observation. Availability flags keep a missing
// field distinct from a real zero.
struct Observation {
    std::string receiver;                 // which wand/watch saw it
    uint32_t atMs = 0;                    // simulated clock

    bool hasRssi = true;
    int8_t rssi = 0;
    bool hasChannel = false;              // BLE advertisements carry no channel
    uint8_t channel = 0;
    SourceKind sourceKind = SourceKind::Ble;
    Band band = Band::Unknown;

    bool hasName = false;
    std::string name;
    std::string address = "aa:bb:cc:dd:ee:ff";
    // First byte 0x02 is locally administered, so it matches no vendor OUI
    // unless a fixture overrides it.
    uint8_t addressBytes[6] = {0x02, 0, 0, 0, 0, 0};

    std::vector<std::string> mfg;         // raw manufacturer records, company id little-endian first
    std::vector<int> uuids;               // 16-bit service UUIDs; -1 = present but not a 16-bit UUID
};

namespace detail {
inline void mfgCb(uint8_t i, std::string &out, const void *ctx)
{
    out = static_cast<const Observation *>(ctx)->mfg[i];
}
inline bool uuidCb(uint8_t i, uint16_t &out, const void *ctx)
{
    const int v = static_cast<const Observation *>(ctx)->uuids[i];
    if (v < 0) return false;
    out = static_cast<uint16_t>(v);
    return true;
}
} // namespace detail

// Present an Observation to the production classifier's input type.
inline recon::BleAdvertSource toSource(const Observation &o)
{
    recon::BleAdvertSource s;
    s.name = o.hasName ? o.name.c_str() : "";
    s.nameLength = o.hasName ? o.name.size() : 0;
    s.printedAddress = o.address.c_str();
    s.addressBytes = o.addressBytes;
    s.rssi = o.hasRssi ? o.rssi : 0;
    s.manufacturerCount = static_cast<uint8_t>(o.mfg.size());
    s.manufacturer = detail::mfgCb;
    s.uuidCount = static_cast<uint8_t>(o.uuids.size());
    s.uuid16 = detail::uuidCb;
    s.context = &o;
    return s;
}

// One real MonitorEventLog per receiver id.
class SensorReplay {
public:
    void feed(const Observation &o, ReconTarget scan)
    {
        Ctx c{&logOf(o.receiver), &o};
        recon::classifyBleAdvert(toSource(o), scan, &SensorReplay::sink, &c);
    }
    void setSleep(const std::string &receiver, bool on) { logOf(receiver).setSleepMode(on); }
    void ack(const std::string &receiver) { logOf(receiver).acknowledgeAlert(); }
    void reset(const std::string &receiver) { logOf(receiver).clear(); }

    recon::MonitorEventLog &logOf(const std::string &receiver) { return _logs[receiver]; }
    bool has(const std::string &receiver) const { return _logs.count(receiver) != 0; }

private:
    struct Ctx { recon::MonitorEventLog *log; const Observation *obs; };
    static void sink(const recon::Candidate &in, void *ctx)
    {
        Ctx *c = static_cast<Ctx *>(ctx);
        recon::Candidate cand = in;         // classifier filled detector/detail/address/rssi/confidence/channel
        cand.sourceKind = c->obs->sourceKind;  // platform stamps these on the way to core
        cand.band = c->obs->band;
        cand.atMs = c->obs->atMs;
        c->log->add(cand);
    }
    std::map<std::string, recon::MonitorEventLog> _logs;
};

// Find a record by (detector, sourceId) in a receiver's history. nullptr when
// absent. sourceId is the observed address text, as the log stores it.
inline const MonitorEvent *find(const recon::MonitorEventLog &log,
                                ReconTarget detector, const char *sourceId)
{
    for (uint8_t i = 0; i < log.count(); ++i) {
        const MonitorEvent &e = log.event(i);
        if (e.detector == detector && std::strcmp(e.sourceId, sourceId) == 0) return &e;
    }
    return nullptr;
}

} // namespace replay
} // namespace layertime
