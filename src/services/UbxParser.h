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

#include <stddef.h>
#include <stdint.h>

// u-blox UBX binary protocol, enough of it to ask the MIA-M10Q how good its
// own fix is.
//
// Why this exists at all: NMEA has no sentence that carries a position
// accuracy. GGA carries HDOP, which is satellite geometry, not error. Measured
// on this watch 2026-09-05, HDOP swung between 1.49 and 3.04 while the
// receiver's own hAcc estimate never left 1.48 to 1.73 metres. Sizing an
// uncertainty circle or an MGRS digit count from HDOP would make the display
// flap while the actual confidence sat still.
//
// The byte offsets below were verified against live NAV-PVT payloads captured
// from this exact receiver (firmware ROM SPG 5.10, PROTVER 34.10), not read
// out of a datasheet and hoped for. hMSL decoded to 400.5 m against a GGA
// altitude of 400.3 m in the same second, which is the cross-check.
namespace Ubx {

constexpr uint8_t kSyncChar1 = 0xB5;
constexpr uint8_t kSyncChar2 = 0x62;

constexpr uint8_t kClassNav = 0x01;
constexpr uint8_t kIdNavPvt = 0x07;

// Largest payload this parser will hold. NAV-PVT is 92 bytes. Anything longer
// is discarded rather than truncated, because a truncated frame that still
// passed a checksum would be worse than no frame at all.
constexpr uint16_t kMaxPayload = 128;

// A decoded UBX-NAV-PVT. Accuracy figures are the receiver's own estimates
// from its navigation filter, which is the whole point of reading UBX.
struct NavPvt {
    uint32_t iTowMs = 0;
    uint8_t fixType = 0;   // 0 none, 1 DR only, 2 2D, 3 3D, 4 GNSS+DR, 5 time only
    bool fixOk = false;    // the receiver's own "this solution is usable" flag
    uint8_t satellites = 0;
    double latitude = 0.0;
    double longitude = 0.0;
    float altitudeMslM = 0.0f;

    // u-blox reports 0xFFFFFFFF when it has no estimate. That is a sentinel,
    // NOT an error of 4295 kilometres, and anything that treats it as a
    // distance will draw an uncertainty circle the size of the planet.
    bool horizontalAccuracyValid = false;
    float horizontalAccuracyM = 0.0f;
    bool verticalAccuracyValid = false;
    float verticalAccuracyM = 0.0f;

    float pdop = 0.0f;
};

// Streaming frame parser. Feed it every byte off the GNSS UART, NMEA included:
// anything that is not a well formed, checksum-valid UBX frame is discarded,
// so one stream can carry both protocols and neither parser has to own the
// port.
class Parser {
public:
    void reset();

    // Returns true only when this byte completed a frame whose checksum
    // verified. A frame that fails its checksum is counted and dropped.
    bool feed(uint8_t byte);

    uint8_t messageClass() const { return _class; }
    uint8_t messageId() const { return _id; }
    const uint8_t *payload() const { return _payload; }
    uint16_t payloadLength() const { return _length; }

    uint32_t framesAccepted() const { return _accepted; }
    uint32_t framesRejected() const { return _rejected; }

private:
    enum class State : uint8_t {
        Sync1, Sync2, Class, Id, LengthLow, LengthHigh, Payload, CkA, CkB
    };

    void startFrame();
    void accumulate(uint8_t byte);

    State _state = State::Sync1;
    uint8_t _class = 0;
    uint8_t _id = 0;
    uint16_t _length = 0;
    uint16_t _filled = 0;
    uint8_t _ckA = 0;
    uint8_t _ckB = 0;
    uint8_t _frameCkA = 0;
    uint8_t _payload[kMaxPayload] = {0};
    uint32_t _accepted = 0;
    uint32_t _rejected = 0;
};

// Decodes a NAV-PVT payload. False if the payload is too short to trust.
bool decodeNavPvt(const uint8_t *payload, uint16_t length, NavPvt &out);

// Builds a zero-length poll frame, which asks the receiver to send that
// message once, now. Polling writes nothing to the receiver's configuration,
// so there is nothing to persist and nothing to undo. Returns bytes written.
size_t buildPoll(uint8_t messageClass, uint8_t messageId, uint8_t *out, size_t outSize);

}
