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

#pragma once

// LayerTime Link 0.1: the constants and frame structs of contracts/link.md.
// Numbers here are the contract; contracts/vectors/link_frames.json is the
// evidence, and test/test_link_codec/ holds this file to it.
//
// Increment 1 bound Status, HELLO/HELLO_ACK, PING/ACK and ERROR. The Recon
// integration (Increment 2B) binds COMMAND/RESULT, GET_CHANGED/EVENT_SUMMARY/
// END and GET_TEXT/TEXT; LinkServer.h answers them. RX_MARK stays unbound.

#include <stddef.h>
#include <stdint.h>

namespace layertime {
namespace link {

constexpr uint8_t kMajor = 0;
constexpr uint8_t kMinor = 1;
constexpr uint8_t kVersionByte = static_cast<uint8_t>((kMajor << 4) | kMinor); // 0x01

constexpr size_t kMaxFrame = 20;   // Connect IQ writes at most 20 bytes
constexpr size_t kStatusSize = 18;

// 128-bit UUIDs, fixed. Text form for stacks that take strings.
constexpr char kServiceUuid[] = "8ac60001-a08b-4851-b89f-e6b39081268e";
constexpr char kControlUuid[] = "8ac60002-a08b-4851-b89f-e6b39081268e";
constexpr char kStatusUuid[] = "8ac60003-a08b-4851-b89f-e6b39081268e";
constexpr char kDataUuid[] = "8ac60004-a08b-4851-b89f-e6b39081268e";
constexpr char kProbeUuid[] = "8ac600f0-a08b-4851-b89f-e6b39081268e"; // test builds only

enum class Op : uint8_t {
    Hello = 0x01,
    Ping = 0x02,
    Command = 0x03,
    GetChanged = 0x04,
    GetText = 0x05,
    TestFirst = 0xF0,
    RxMark = 0xF1,      // bound in Increment 2B
    TestLast = 0xFE,
};

enum class FrameType : uint8_t {
    HelloAck = 0x81,
    Ack = 0x82,
    Result = 0x83,
    EventSummary = 0x84,
    End = 0x85,
    Text = 0x86,
    Error = 0x8F,
};

enum class LinkStatus : uint8_t {
    Ok = 0,
    UnknownOp = 1,
    BadLength = 2,
    VersionMismatch = 3,
    Busy = 4,
};

enum class Schedule : uint8_t {
    Simultaneous = 0,
    Recon = 1,
    Report = 2,
};

// Status.flags bits.
constexpr uint8_t kFlagMonitoring = 0x01;
constexpr uint8_t kFlagEarlyWarningEnabled = 0x02;
constexpr uint8_t kFlagEarlyWarningResting = 0x04;
constexpr uint8_t kFlagAlertPending = 0x08;
constexpr uint8_t kFlagSleepMode = 0x10;
// The Node keeps its events only in memory: no SD card log (LayerWand,
// 2026-10-03). The watch warns the user. Never set by a Link-only Node.
constexpr uint8_t kFlagNoSdLog = 0x20;

// HELLO_ACK.capabilities bits.
constexpr uint16_t kCapLocalWifiMonitor = 0x0001;
constexpr uint16_t kCapLocalBleMonitor = 0x0002;
constexpr uint16_t kCapDisplay = 0x0004;
constexpr uint16_t kCapLed = 0x0008;
constexpr uint16_t kCapButton = 0x0010;

// Request and reply sizes on the wire.
constexpr size_t kHelloSize = 4;
constexpr size_t kPingSize = 6;
constexpr size_t kHelloAckSize = 10;
constexpr size_t kAckSize = 8;
constexpr size_t kErrorSize = 3;
constexpr size_t kCommandMinSize = 3;   // op, reqId, commandType
constexpr size_t kGetChangedSize = 6;
constexpr size_t kGetTextSize = 7;
constexpr size_t kResultSize = 5;
constexpr size_t kEventSummarySize = 18;
constexpr size_t kEndSize = 9;
constexpr size_t kTextHeaderSize = 6;
constexpr size_t kTextMaxChunk = kMaxFrame - kTextHeaderSize; // 14

// GET_TEXT / TEXT field numbers.
enum class TextField : uint8_t {
    SourceId = 0,
    Detail = 1,
};

// EVENT_SUMMARY.flags bits.
constexpr uint8_t kSummaryHasSourceId = 0x01;
constexpr uint8_t kSummaryHasDetail = 0x02;

// The probe payload sizes for the notification-size measurement.
constexpr size_t kProbeSizes[] = {20, 21, 64, 180};
constexpr size_t kProbeSizeCount = sizeof(kProbeSizes) / sizeof(kProbeSizes[0]);
constexpr size_t kProbeMaxSize = 180;

struct StatusSnapshot {
    uint8_t linkVersion = kVersionByte;
    uint16_t sessionId = 0;
    uint32_t changeSeq = 0;
    uint8_t flags = 0;
    uint8_t selected = 0;
    uint8_t active = 0;
    uint8_t eventCount = 0;
    uint32_t lastAlertEventId = 0;
    uint8_t heartbeat = 0;
    Schedule schedule = Schedule::Simultaneous;
    uint8_t nextReportS = 0;
};

struct Hello {
    uint8_t reqId = 0;
    uint8_t clientMajor = 0;
    uint8_t clientMinor = 0;
};

struct Ping {
    uint8_t reqId = 0;
    uint32_t token = 0;
};

struct HelloAck {
    uint8_t reqId = 0;
    LinkStatus status = LinkStatus::Ok;
    uint8_t serverMajor = kMajor;
    uint8_t serverMinor = kMinor;
    uint16_t sessionId = 0;
    uint16_t capabilities = 0;
    uint8_t maxFrame = static_cast<uint8_t>(kMaxFrame);
};

struct Ack {
    uint8_t reqId = 0;
    LinkStatus status = LinkStatus::Ok;
    uint32_t token = 0;
    uint8_t heartbeat = 0;
};

struct Error {
    uint8_t reqId = 0;
    LinkStatus status = LinkStatus::Ok;
};

// COMMAND: 3 bytes, or 4 when the command takes an argument.
struct Command {
    uint8_t reqId = 0;
    uint8_t commandType = 0;
    bool hasArgument = false;
    uint8_t argument = 0;
};

struct GetChanged {
    uint8_t reqId = 0;
    uint32_t sinceChangeSeq = 0;
};

struct GetText {
    uint8_t reqId = 0;
    uint32_t eventId = 0;
    uint8_t field = 0;
};

struct Result {
    uint8_t reqId = 0;
    LinkStatus status = LinkStatus::Ok;
    uint8_t commandType = 0;
    uint8_t commandResult = 0;
};

struct EventSummary {
    uint8_t reqId = 0;
    LinkStatus status = LinkStatus::Ok;
    uint32_t eventId = 0;
    uint8_t detector = 0;
    uint8_t confidence = 0;
    uint8_t sourceKind = 0;
    uint8_t band = 0;
    uint8_t channel = 0;
    int8_t rssi = 0;
    uint16_t count = 0;
    uint16_t ageSeconds = 0;
    uint8_t flags = 0;
};

struct End {
    uint8_t reqId = 0;
    LinkStatus status = LinkStatus::Ok;
    uint8_t count = 0;
    uint8_t gap = 0;
    uint32_t changeSeq = 0;
};

struct Text {
    uint8_t reqId = 0;
    LinkStatus status = LinkStatus::Ok;
    uint8_t field = 0;
    uint8_t index = 0;
    uint8_t total = 0;
    uint8_t length = 0; // 1..kTextMaxChunk
    uint8_t bytes[kTextMaxChunk] = {};
};

} // namespace link
} // namespace layertime
