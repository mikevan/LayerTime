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

// LayerTime Link 0.1 codec and Node-side dispatcher (contracts/link.md).
// Pure C++: no stack, no clock, no hardware. The platform hands in the bytes
// it received and sends back the bytes this returns.

#include <stddef.h>
#include <stdint.h>

#include "LinkFrames.h"

namespace layertime {
namespace link {

// Encoders write exactly the contract size and return it. out must hold at
// least kMaxFrame bytes.
size_t encodeStatus(const StatusSnapshot &s, uint8_t *out);
size_t encodeHello(const Hello &h, uint8_t *out);
size_t encodePing(const Ping &p, uint8_t *out);
size_t encodeHelloAck(const HelloAck &a, uint8_t *out);
size_t encodeAck(const Ack &a, uint8_t *out);
size_t encodeError(const Error &e, uint8_t *out);

// Decoders return false when the length is wrong or the type byte does not
// match. On false the output is untouched.
bool decodeStatus(const uint8_t *in, size_t len, StatusSnapshot &out);
bool decodeHello(const uint8_t *in, size_t len, Hello &out);
bool decodePing(const uint8_t *in, size_t len, Ping &out);
bool decodeHelloAck(const uint8_t *in, size_t len, HelloAck &out);
bool decodeAck(const uint8_t *in, size_t len, Ack &out);
bool decodeError(const uint8_t *in, size_t len, Error &out);

// What the Node reports about itself in HELLO_ACK.
struct NodeIdentity {
    uint16_t sessionId = 0;
    uint16_t capabilities = 0;
    bool testBuild = false;
};

// Answers one Control request with exactly one Data frame. reply must hold
// kMaxFrame bytes; the return value is the reply length, never 0.
//   heartbeat: the Node's current heartbeat, echoed in ACK.
// Ops not bound in this increment are answered with ERROR UnknownOp, as are
// test-range ops on a release build.
size_t dispatch(const NodeIdentity &node, uint8_t heartbeat, const uint8_t *request, size_t len,
                uint8_t *reply);

// Fills a probe payload of the given size: byte 0 is size mod 256, byte i is
// i mod 256. Returns the size, or 0 if size is not one of kProbeSizes.
size_t fillProbe(size_t size, uint8_t *out);

} // namespace link
} // namespace layertime
