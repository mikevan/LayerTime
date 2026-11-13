// LayerTime - counter-intrusion and resilient-communications firmware.
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

// One evidence window as a pcap file (Michael, 2026-11-04), so the Hunter can
// keep the frames behind each detection on its SD card.
//
// The format is the one LayerScope writes when it pulls a window over the Link
// (LinkEvidence.toPcap with FrameFabricator.radiotap): classic pcap, link type
// 127 (802.11 plus radiotap), and a 16-byte radiotap header per frame carrying
// the channel's frequency and the signal. The frames have no FCS (the C5's
// trailing 4 bytes are stripped before they are kept). The Hunter has no
// clock, so a frame's time is its uptime in ms; the log's boot line anchors it.
// The one difference from the phone's file: the phone adds the wall-clock time
// of the pull, the Hunter writes time since boot.
//
// Pure: it reads the store and fills a caller's buffer, so the platform copies
// the window under its evidence lock and writes the card after letting go.

#include <stddef.h>
#include <stdint.h>

#include "EvidenceStore.h"

namespace layertime {
namespace evidence {

constexpr size_t kPcapHeaderSize = 24;
constexpr size_t kPcapRecordHeaderSize = 16;
constexpr size_t kRadiotapSize = 16;
constexpr uint32_t kPcapLinkTypeRadiotap = 127;

// Bytes the largest window needs: header plus kWindowFrames full frames.
constexpr size_t kMaxWindowPcapBytes =
    kPcapHeaderSize + static_cast<size_t>(kWindowFrames) * (kPcapRecordHeaderSize + kRadiotapSize + kMaxFrameBytes);

// The channel's centre frequency in MHz: 2.4 GHz for 1 to 14, else 5 GHz.
uint16_t channelMhz(uint8_t channel);

// Writes the 24-byte file header. Returns kPcapHeaderSize.
size_t pcapHeader(uint8_t out[kPcapHeaderSize]);

// Writes the 16-byte radiotap header for one frame. Returns kRadiotapSize.
size_t radiotap(uint8_t channel, int8_t rssi, uint8_t out[kRadiotapSize]);

// The whole window behind `eventId` as a pcap file in `out`. Returns the length,
// or 0 when there is no such window or `cap` is too small for it (then nothing
// useful is in `out`). `info` gets the window's counts.
size_t buildWindowPcap(const EvidenceStore &store, uint32_t eventId, uint32_t nowMs, uint8_t *out, size_t cap,
                       WindowInfo &info);

} // namespace evidence
} // namespace layertime
