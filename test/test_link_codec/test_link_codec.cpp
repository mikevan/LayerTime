// LayerTime Link 0.1 conformance: the C++ codec and Node dispatcher against
// contracts/vectors/link_frames.json. Every vector is byte-exact, so this
// suite is what makes the C++ and Monkey C bindings agree.
//
// Build command: see test/README.md, "LayerTime Link".

#include "check.h"
#include "link_vectors_json.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "core/link/LinkCodec.h"
#include "core/link/LinkFrames.h"

using namespace layertime::link;

// --- Constants match the vectors -----------------------------------------

void constants_match_the_vectors()
{
    const Json &c = vectors()["constants"];
    CHECK_INT(c["maxFrame"].asLong(), static_cast<long>(kMaxFrame));
    CHECK_INT(c["statusSize"].asLong(), static_cast<long>(kStatusSize));
    CHECK_INT(c["linkVersionByte"].asLong(), kVersionByte);
    CHECK_INT(c["serverMajor"].asLong(), kMajor);
    CHECK_INT(c["serverMinor"].asLong(), kMinor);
    const Json &ops = c["ops"];
    CHECK_INT(ops["HELLO"].asLong(), static_cast<long>(Op::Hello));
    CHECK_INT(ops["PING"].asLong(), static_cast<long>(Op::Ping));
    CHECK_INT(ops["COMMAND"].asLong(), static_cast<long>(Op::Command));
    CHECK_INT(ops["GET_CHANGED"].asLong(), static_cast<long>(Op::GetChanged));
    CHECK_INT(ops["GET_TEXT"].asLong(), static_cast<long>(Op::GetText));
    CHECK_INT(ops["testRangeFirst"].asLong(), static_cast<long>(Op::TestFirst));
    CHECK_INT(ops["testRangeLast"].asLong(), static_cast<long>(Op::TestLast));
    CHECK_INT(ops["RX_MARK"].asLong(), static_cast<long>(Op::RxMark));
    const Json &ft = c["frameTypes"];
    CHECK_INT(ft["HELLO_ACK"].asLong(), static_cast<long>(FrameType::HelloAck));
    CHECK_INT(ft["ACK"].asLong(), static_cast<long>(FrameType::Ack));
    CHECK_INT(ft["RESULT"].asLong(), static_cast<long>(FrameType::Result));
    CHECK_INT(ft["EVENT_SUMMARY"].asLong(), static_cast<long>(FrameType::EventSummary));
    CHECK_INT(ft["END"].asLong(), static_cast<long>(FrameType::End));
    CHECK_INT(ft["TEXT"].asLong(), static_cast<long>(FrameType::Text));
    CHECK_INT(ft["ERROR"].asLong(), static_cast<long>(FrameType::Error));
    const Json &ls = c["linkStatus"];
    CHECK_INT(ls["Ok"].asLong(), static_cast<long>(LinkStatus::Ok));
    CHECK_INT(ls["UnknownOp"].asLong(), static_cast<long>(LinkStatus::UnknownOp));
    CHECK_INT(ls["BadLength"].asLong(), static_cast<long>(LinkStatus::BadLength));
    CHECK_INT(ls["VersionMismatch"].asLong(), static_cast<long>(LinkStatus::VersionMismatch));
    CHECK_INT(ls["Busy"].asLong(), static_cast<long>(LinkStatus::Busy));
    const Json &fl = c["statusFlags"];
    CHECK_INT(fl["monitoring"].asLong(), kFlagMonitoring);
    CHECK_INT(fl["earlyWarningEnabled"].asLong(), kFlagEarlyWarningEnabled);
    CHECK_INT(fl["earlyWarningResting"].asLong(), kFlagEarlyWarningResting);
    CHECK_INT(fl["alertPending"].asLong(), kFlagAlertPending);
    CHECK_INT(fl["sleepMode"].asLong(), kFlagSleepMode);
    const Json &cap = c["capabilities"];
    CHECK_INT(cap["localWifiMonitor"].asLong(), kCapLocalWifiMonitor);
    CHECK_INT(cap["localBleMonitor"].asLong(), kCapLocalBleMonitor);
    CHECK_INT(cap["display"].asLong(), kCapDisplay);
    CHECK_INT(cap["led"].asLong(), kCapLed);
    CHECK_INT(cap["button"].asLong(), kCapButton);
    const Json &sizes = c["probeSizes"];
    CHECK_INT(static_cast<long>(kProbeSizeCount), static_cast<long>(sizes.items.size()));
    for (size_t i = 0; i < sizes.items.size() && i < kProbeSizeCount; ++i) CHECK_INT(sizes.items[i].asLong(), static_cast<long>(kProbeSizes[i]));
    const Json &u = vectors()["uuids"];
    CHECK_STR(u["service"].str.c_str(), kServiceUuid);
    CHECK_STR(u["control"].str.c_str(), kControlUuid);
    CHECK_STR(u["status"].str.c_str(), kStatusUuid);
    CHECK_STR(u["data"].str.c_str(), kDataUuid);
    CHECK_STR(u["probe"].str.c_str(), kProbeUuid);
}

// --- Status ---------------------------------------------------------------

void status_encodes_and_decodes_every_vector()
{
    for (const Json &v : vectors()["status"].items) {
        const Json &f = v["fields"];
        StatusSnapshot s;
        s.linkVersion = static_cast<uint8_t>(f["linkVersion"].asLong());
        s.sessionId = static_cast<uint16_t>(f["sessionId"].asULong());
        s.changeSeq = static_cast<uint32_t>(f["changeSeq"].asULong());
        s.flags = static_cast<uint8_t>(f["flags"].asLong());
        s.selected = static_cast<uint8_t>(f["selected"].asLong());
        s.active = static_cast<uint8_t>(f["active"].asLong());
        s.eventCount = static_cast<uint8_t>(f["eventCount"].asLong());
        s.lastAlertEventId = static_cast<uint32_t>(f["lastAlertEventId"].asULong());
        s.heartbeat = static_cast<uint8_t>(f["heartbeat"].asLong());
        s.schedule = static_cast<Schedule>(f["schedule"].asLong());
        s.nextReportS = static_cast<uint8_t>(f["nextReportS"].asLong());
        uint8_t buf[kMaxFrame] = {};
        const size_t n = encodeStatus(s, buf);
        CHECK_INT(static_cast<long>(kStatusSize), static_cast<long>(n));
        CHECK_STR(v["bytes"].str.c_str(), toHex(buf, n).c_str());

        StatusSnapshot d;
        const std::vector<uint8_t> in = fromHex(v["bytes"].str);
        CHECK_TRUE(decodeStatus(in.data(), in.size(), d));
        CHECK_INT(s.sessionId, d.sessionId);
        CHECK_INT(static_cast<long>(s.changeSeq), static_cast<long>(d.changeSeq));
        CHECK_INT(s.flags, d.flags);
        CHECK_INT(s.selected, d.selected);
        CHECK_INT(s.active, d.active);
        CHECK_INT(s.eventCount, d.eventCount);
        CHECK_INT(static_cast<long>(s.lastAlertEventId), static_cast<long>(d.lastAlertEventId));
        CHECK_INT(s.heartbeat, d.heartbeat);
        CHECK_INT(static_cast<int>(s.schedule), static_cast<int>(d.schedule));
        CHECK_INT(s.nextReportS, d.nextReportS);
    }
}

void status_rejects_wrong_lengths()
{
    uint8_t buf[kMaxFrame] = {};
    StatusSnapshot d;
    CHECK_FALSE(decodeStatus(buf, 17, d));
    CHECK_FALSE(decodeStatus(buf, 19, d));
    CHECK_FALSE(decodeStatus(buf, 0, d));
}

// --- Requests and replies ------------------------------------------------

void requests_encode_and_decode_every_vector()
{
    for (const Json &v : vectors()["requests"].items) {
        const Json &f = v["fields"];
        const std::vector<uint8_t> in = fromHex(v["bytes"].str);
        uint8_t buf[kMaxFrame] = {};
        if (v["op"].str == "HELLO") {
            Hello h;
            h.reqId = static_cast<uint8_t>(f["reqId"].asLong());
            h.clientMajor = static_cast<uint8_t>(f["clientMajor"].asLong());
            h.clientMinor = static_cast<uint8_t>(f["clientMinor"].asLong());
            const size_t n = encodeHello(h, buf);
            CHECK_STR(v["bytes"].str.c_str(), toHex(buf, n).c_str());
            Hello d;
            CHECK_TRUE(decodeHello(in.data(), in.size(), d));
            CHECK_INT(h.reqId, d.reqId);
            CHECK_INT(h.clientMajor, d.clientMajor);
            CHECK_INT(h.clientMinor, d.clientMinor);
        } else if (v["op"].str == "PING") {
            Ping p;
            p.reqId = static_cast<uint8_t>(f["reqId"].asLong());
            p.token = static_cast<uint32_t>(f["token"].asULong());
            const size_t n = encodePing(p, buf);
            CHECK_STR(v["bytes"].str.c_str(), toHex(buf, n).c_str());
            Ping d;
            CHECK_TRUE(decodePing(in.data(), in.size(), d));
            CHECK_INT(p.reqId, d.reqId);
            CHECK_INT(static_cast<long>(p.token), static_cast<long>(d.token));
        } else {
            CHECK_TRUE(false && "unknown request op in vectors");
        }
    }
}

void replies_encode_and_decode_every_vector()
{
    for (const Json &v : vectors()["replies"].items) {
        const Json &f = v["fields"];
        const std::vector<uint8_t> in = fromHex(v["bytes"].str);
        uint8_t buf[kMaxFrame] = {};
        const std::string type = v["type"].str;
        if (type == "HELLO_ACK") {
            HelloAck a;
            a.reqId = static_cast<uint8_t>(f["reqId"].asLong());
            a.status = static_cast<LinkStatus>(f["linkStatus"].asLong());
            a.serverMajor = static_cast<uint8_t>(f["serverMajor"].asLong());
            a.serverMinor = static_cast<uint8_t>(f["serverMinor"].asLong());
            a.sessionId = static_cast<uint16_t>(f["sessionId"].asULong());
            a.capabilities = static_cast<uint16_t>(f["capabilities"].asULong());
            a.maxFrame = static_cast<uint8_t>(f["maxFrame"].asLong());
            const size_t n = encodeHelloAck(a, buf);
            CHECK_STR(v["bytes"].str.c_str(), toHex(buf, n).c_str());
            HelloAck d;
            CHECK_TRUE(decodeHelloAck(in.data(), in.size(), d));
            CHECK_INT(a.reqId, d.reqId);
            CHECK_INT(static_cast<int>(a.status), static_cast<int>(d.status));
            CHECK_INT(a.sessionId, d.sessionId);
            CHECK_INT(a.capabilities, d.capabilities);
            CHECK_INT(a.maxFrame, d.maxFrame);
        } else if (type == "ACK") {
            Ack a;
            a.reqId = static_cast<uint8_t>(f["reqId"].asLong());
            a.status = static_cast<LinkStatus>(f["linkStatus"].asLong());
            a.token = static_cast<uint32_t>(f["token"].asULong());
            a.heartbeat = static_cast<uint8_t>(f["heartbeat"].asLong());
            const size_t n = encodeAck(a, buf);
            CHECK_STR(v["bytes"].str.c_str(), toHex(buf, n).c_str());
            Ack d;
            CHECK_TRUE(decodeAck(in.data(), in.size(), d));
            CHECK_INT(a.reqId, d.reqId);
            CHECK_INT(static_cast<long>(a.token), static_cast<long>(d.token));
            CHECK_INT(a.heartbeat, d.heartbeat);
        } else if (type == "ERROR") {
            Error e;
            e.reqId = static_cast<uint8_t>(f["reqId"].asLong());
            e.status = static_cast<LinkStatus>(f["linkStatus"].asLong());
            const size_t n = encodeError(e, buf);
            CHECK_STR(v["bytes"].str.c_str(), toHex(buf, n).c_str());
            Error d;
            CHECK_TRUE(decodeError(in.data(), in.size(), d));
            CHECK_INT(e.reqId, d.reqId);
            CHECK_INT(static_cast<int>(e.status), static_cast<int>(d.status));
        } else {
            CHECK_TRUE(false && "unknown reply type in vectors");
        }
    }
}

void decoders_reject_the_wrong_type_byte()
{
    uint8_t ack[kAckSize] = {static_cast<uint8_t>(FrameType::Ack), 1, 0, 0, 0, 0, 0, 0};
    HelloAck ha;
    CHECK_FALSE(decodeHelloAck(ack, kAckSize, ha));
    uint8_t hello[kHelloSize] = {static_cast<uint8_t>(Op::Hello), 1, 0, 1};
    Ping p;
    CHECK_FALSE(decodePing(hello, kHelloSize, p));
    Error e;
    CHECK_FALSE(decodeError(ack, kErrorSize, e));
}

// --- Dispatch -------------------------------------------------------------

void the_node_answers_every_dispatch_vector_exactly()
{
    for (const Json &v : vectors()["dispatch"].items) {
        const Json &n = v["node"];
        NodeIdentity node;
        node.sessionId = static_cast<uint16_t>(n["sessionId"].asULong());
        node.capabilities = static_cast<uint16_t>(n["capabilities"].asULong());
        node.testBuild = n["testBuild"].b;
        const std::vector<uint8_t> req = fromHex(v["request"].str);
        uint8_t reply[kMaxFrame] = {};
        const size_t len = dispatch(node, static_cast<uint8_t>(n["heartbeat"].asLong()), req.data(), req.size(), reply);
        CHECK_TRUE(len > 0 && len <= kMaxFrame);
        CHECK_STR(v["reply"].str.c_str(), toHex(reply, len).c_str());
    }
}

void every_frame_the_node_can_send_fits_twenty_bytes()
{
    CHECK_TRUE(kStatusSize <= kMaxFrame);
    CHECK_TRUE(kHelloAckSize <= kMaxFrame);
    CHECK_TRUE(kAckSize <= kMaxFrame);
    CHECK_TRUE(kErrorSize <= kMaxFrame);
    CHECK_TRUE(kHelloSize <= kMaxFrame);
    CHECK_TRUE(kPingSize <= kMaxFrame);
    // A request of the maximum frame size with an unknown op still gets a
    // 3-byte ERROR, never anything longer.
    uint8_t req[kMaxFrame];
    memset(req, 0x7E, sizeof(req));
    uint8_t reply[kMaxFrame];
    NodeIdentity node;
    CHECK_INT(static_cast<long>(kErrorSize), static_cast<long>(dispatch(node, 0, req, kMaxFrame, reply)));
}

// --- Probe ----------------------------------------------------------------

void probe_payloads_have_the_documented_shape()
{
    uint8_t buf[kProbeMaxSize] = {};
    for (size_t i = 0; i < kProbeSizeCount; ++i) {
        const size_t n = fillProbe(kProbeSizes[i], buf);
        CHECK_INT(static_cast<long>(kProbeSizes[i]), static_cast<long>(n));
        CHECK_INT(static_cast<int>(kProbeSizes[i] & 0xFF), buf[0]);
        CHECK_INT(1, buf[1]);
        CHECK_INT(static_cast<int>((n - 1) & 0xFF), buf[n - 1]);
    }
    CHECK_INT(0, static_cast<long>(fillProbe(19, buf)));
    CHECK_INT(0, static_cast<long>(fillProbe(181, buf)));
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(constants_match_the_vectors);
    CASE(status_encodes_and_decodes_every_vector);
    CASE(status_rejects_wrong_lengths);
    CASE(requests_encode_and_decode_every_vector);
    CASE(replies_encode_and_decode_every_vector);
    CASE(decoders_reject_the_wrong_type_byte);
    CASE(the_node_answers_every_dispatch_vector_exactly);
    CASE(every_frame_the_node_can_send_fits_twenty_bytes);
    CASE(probe_payloads_have_the_documented_shape);
    CHECK_SUMMARY();
}
