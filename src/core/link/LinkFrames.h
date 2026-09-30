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
// Increment 1 binds Status, HELLO/HELLO_ACK, PING/ACK and ERROR. The other
// operations are named so their numbers are fixed, and are answered with
// UnknownOp until their increments bind them.

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
    Command = 0x03,     // bound in Increment 4
    GetChanged = 0x04,  // bound in Increment 5
    GetText = 0x05,     // bound in Increment 5
    TestFirst = 0xF0,
    RxMark = 0xF1,      // bound in Increment 2B
    TestLast = 0xFE,
};

enum class FrameType : uint8_t {
    HelloAck = 0x81,
    Ack = 0x82,
    Result = 0x83,        // Increment 4
    EventSummary = 0x84,  // Increment 5
    End = 0x85,           // Increment 5
    Text = 0x86,          // Increment 5
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

} // namespace link
} // namespace layertime
