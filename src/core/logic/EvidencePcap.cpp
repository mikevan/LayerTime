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

#include "EvidencePcap.h"

namespace layertime {
namespace evidence {

namespace {

void le16(uint8_t *p, uint16_t v)
{
    p[0] = static_cast<uint8_t>(v & 0xFF);
    p[1] = static_cast<uint8_t>(v >> 8);
}

void le32(uint8_t *p, uint32_t v)
{
    for (int i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>((v >> (8 * i)) & 0xFF);
}

} // namespace

uint16_t channelMhz(uint8_t channel)
{
    if (channel == 14) return 2484;
    if (channel >= 1 && channel <= 13) return static_cast<uint16_t>(2407 + channel * 5);
    return static_cast<uint16_t>(5000 + channel * 5);
}

size_t pcapHeader(uint8_t out[kPcapHeaderSize])
{
    le32(out, 0xA1B2C3D4u); // microsecond timestamps, little-endian
    le16(out + 4, 2);       // version 2.4
    le16(out + 6, 4);
    le32(out + 8, 0);       // GMT offset
    le32(out + 12, 0);      // timestamp accuracy
    le32(out + 16, 65535);  // snap length
    le32(out + 20, kPcapLinkTypeRadiotap);
    return kPcapHeaderSize;
}

size_t radiotap(uint8_t channel, int8_t rssi, uint8_t out[kRadiotapSize])
{
    // Same bytes as LayerScope's FrameFabricator.radiotap: version 0, length 16,
    // present = flags | channel | antenna signal (0x2A).
    const uint16_t freq = channelMhz(channel);
    const uint16_t flags = channel <= 14 ? 0x0480 : 0x0140; // 2 GHz + CCK, or 5 GHz + OFDM
    out[0] = 0;
    out[1] = 0;
    le16(out + 2, kRadiotapSize);
    le32(out + 4, 0x0000002Au);
    out[8] = 0;  // flags: no FCS
    out[9] = 0;  // pad to align the channel field
    le16(out + 10, freq);
    le16(out + 12, flags);
    out[14] = static_cast<uint8_t>(rssi);
    out[15] = 0; // pad
    return kRadiotapSize;
}

size_t buildWindowPcap(const EvidenceStore &store, uint32_t eventId, uint32_t nowMs, uint8_t *out, size_t cap,
                       WindowInfo &info)
{
    if (out == nullptr || !store.info(eventId, nowMs, info)) return 0;
    if (cap < kPcapHeaderSize) return 0;
    size_t n = pcapHeader(out);
    for (uint8_t i = 0; i < info.retained; ++i) {
        FrameMeta m;
        int32_t offsetMs = 0;
        if (!store.frameMeta(eventId, i, m, offsetMs)) return 0;
        const size_t need = kPcapRecordHeaderSize + kRadiotapSize + m.captured;
        if (n + need > cap) return 0;
        uint8_t *rec = out + n;
        le32(rec, m.atMs / 1000u);
        le32(rec + 4, (m.atMs % 1000u) * 1000u);
        le32(rec + 8, static_cast<uint32_t>(kRadiotapSize + m.captured));
        const uint16_t onAir = m.onAir > m.captured ? m.onAir : m.captured;
        le32(rec + 12, static_cast<uint32_t>(kRadiotapSize + onAir));
        radiotap(m.channel, m.rssi, rec + kPcapRecordHeaderSize);
        const size_t copied = store.copyBytes(eventId, i, 0, rec + kPcapRecordHeaderSize + kRadiotapSize, m.captured);
        if (copied != m.captured) return 0;
        n += need;
    }
    return n;
}

} // namespace evidence
} // namespace layertime
