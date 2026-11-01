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

// HUNT v1 (claude/hunt_v1_layervixen_plan.md, a slice of Link 0.2 Part 3 in
// contracts/link_0_2.md): the HUNT_START, HUNT_STOP and GET_HUNT ops, the
// HUNT_READING frame, and HuntTracker, which turns sightings of the target
// into one reading per poll.
//
// v1 hunts one kind of target: any LayerVixen, the beacon that advertises
// kVixenServiceUuid (target kind kTargetAnyVixen, Deviation 1 of the plan).
// Hunting a real detection by its address comes with the Bluetooth
// byte-order fix.
//
// Pure C++: no stack, no clock, no hardware. Host-tested
// (test/test_link_hunt).

#include <stddef.h>
#include <stdint.h>

#include "LinkFrames.h"

namespace layertime {
namespace link {
namespace hunt {

constexpr uint8_t kOpHuntStart = 0x08;
constexpr uint8_t kOpHuntStop = 0x09;
constexpr uint8_t kOpGetHunt = 0x0A;
constexpr uint8_t kTypeHuntReading = 0x8A;

constexpr size_t kHuntStartSize = 10;
constexpr size_t kHuntStopSize = 2;
constexpr size_t kGetHuntSize = 2;
constexpr size_t kHuntReadingSize = 14;
static_assert(kHuntReadingSize <= kMaxFrame, "HUNT_READING must fit one frame");

// HELLO_ACK capability bit 6 and Status flag bit 6 (Link 0.2 Part 3).
constexpr uint16_t kCapHunt = 0x0040;
constexpr uint8_t kFlagHunting = 0x40;

// The LayerVixen beacon's service, in the LayerTime UUID family. Not the
// Link service: watches never connect to a Vixen and LayerWands never count
// one as a node.
constexpr char kVixenServiceUuid[] = "8ac60010-a08b-4851-b89f-e6b39081268e";

// HUNT_START target kinds. v1 supports only kTargetAnyVixen.
constexpr uint8_t kTargetAnyVixen = 0xF0;

// RESULT.commandResult for HUNT_START.
constexpr uint8_t kStarted = 0;
constexpr uint8_t kUnsupportedTarget = 1;

// HUNT_READING signal fields when there was no sighting.
constexpr int8_t kNoReading = -128;

// HUNT_READING flags.
constexpr uint8_t kReadingHunting = 0x01;
constexpr uint8_t kReadingLost = 0x02;   // no sighting for kLostAfterMs
constexpr uint8_t kReadingLocked = 0x04; // locked on a Wi-Fi channel (never in v1)

constexpr uint32_t kLostAfterMs = 5000;

// HUNT_START: op, reqId, targetKind, address (6 bytes, most significant
// first), channel (Wi-Fi; 0 = hop). The address and channel are ignored for
// kTargetAnyVixen.
struct HuntStart {
    uint8_t reqId = 0;
    uint8_t targetKind = 0;
    uint8_t address[6] = {0, 0, 0, 0, 0, 0};
    uint8_t channel = 0;
};

// HUNT_READING: type, reqId, status, readings u16, latest, strongest,
// weakest and mean (i8 dBm over the readings since the last GET_HUNT, or
// kNoReading), msSinceHeard u16 (saturating; 0xFFFF when never heard),
// channel, flags. 14 bytes, little-endian.
struct HuntReading {
    uint8_t reqId = 0;
    LinkStatus status = LinkStatus::Ok;
    uint16_t readings = 0;
    int8_t latest = kNoReading;
    int8_t strongest = kNoReading;
    int8_t weakest = kNoReading;
    int8_t mean = kNoReading;
    uint16_t msSinceHeard = 0xFFFF;
    uint8_t channel = 0;
    uint8_t flags = 0;
};

size_t encodeHuntStart(const HuntStart &in, uint8_t *out);
bool decodeHuntStart(const uint8_t *in, size_t len, HuntStart &out);
// HUNT_STOP and GET_HUNT carry only op and reqId.
size_t encodeHuntStop(uint8_t reqId, uint8_t *out);
size_t encodeGetHunt(uint8_t reqId, uint8_t *out);
size_t encodeHuntReading(const HuntReading &in, uint8_t *out);
bool decodeHuntReading(const uint8_t *in, size_t len, HuntReading &out);

// True for the three HUNT ops.
bool isHuntOp(uint8_t op);

// The target's sightings, turned into one reading per GET_HUNT. Not
// thread-safe: the platform guards it (sightings arrive on the Bluetooth
// task, polls on the loop).
class HuntTracker {
public:
    void start(uint32_t nowMs);
    void stop();
    bool active() const { return _active; }

    // One advertisement from the target, at rssi dBm. Ignored when no hunt
    // is running. rssi is clamped to -127 .. 20.
    void sighting(int rssi, uint32_t nowMs);

    // The reading since the last take(), which starts the next window.
    HuntReading take(uint8_t reqId, uint32_t nowMs);

    // Sightings since start(), for the serial report.
    uint32_t totalSightings() const { return _total; }

private:
    bool _active = false;
    bool _heard = false;
    uint32_t _startMs = 0;
    uint32_t _lastHeardMs = 0;
    uint32_t _total = 0;
    uint16_t _count = 0;
    int32_t _sum = 0;
    int8_t _latest = kNoReading;
    int8_t _strongest = kNoReading;
    int8_t _weakest = kNoReading;
};

// Answers one HUNT request with exactly one reply frame (RESULT or
// HUNT_READING, or ERROR BadLength), and starts or stops the tracker.
// Starting and stopping the radios is the platform's job: it compares
// tracker.active() before and after. reply must hold kMaxFrame bytes.
size_t answer(HuntTracker &tracker, const uint8_t *request, size_t len, uint32_t nowMs, uint8_t *reply);

} // namespace hunt
} // namespace link
} // namespace layertime
