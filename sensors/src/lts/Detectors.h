// LayerTime-Sensors - passive wireless threat detectors for small radios.
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

// The detector vocabulary: which detector matched, how sure it is, which
// radio saw it, and on which band. Every value here is a stable number.
// Consumers store and transmit these numbers, so a value is never reused or
// renumbered; a retired detector leaves a gap, and a new one takes the next
// unused number.
//
// Detector numbers start at 5 and are sparse on purpose: they are the values
// these detectors carried in the firmware the library was extracted from,
// which kept 0 to 4 for its own menu entries. Code must never assume the
// numbers are contiguous or start at zero; use kAllDetectors and
// isKnownDetector() instead of ranges.

#include <stddef.h>
#include <stdint.h>

namespace lts {

enum class DetectorId : uint8_t {
    Deauth = 5,
    Pwnagotchi = 6,
    MultiSSID = 7,
    Flock = 8,
    Pineapple = 9,
    AirTag = 10,
    Flipper = 11,
    Meta = 12,
    Axon = 13,
    Tile = 14,
    SamsungTag = 15,
    GoogleTag = 16,
};

inline constexpr DetectorId kAllDetectors[] = {
    DetectorId::Deauth,  DetectorId::Pwnagotchi, DetectorId::MultiSSID,
    DetectorId::Flock,   DetectorId::Pineapple,  DetectorId::AirTag,
    DetectorId::Flipper, DetectorId::Meta,       DetectorId::Axon,
    DetectorId::Tile,    DetectorId::SamsungTag, DetectorId::GoogleTag,
};

inline constexpr size_t kDetectorCount = sizeof(kAllDetectors) / sizeof(kAllDetectors[0]);

// True only for a number that names a detector in this version.
constexpr bool isKnownDetector(uint8_t value)
{
    for (DetectorId d : kAllDetectors)
        if (static_cast<uint8_t>(d) == value) return true;
    return false;
}

enum class Confidence : uint8_t {
    Low = 0,
    Medium = 1,
    High = 2,
};

enum class SourceKind : uint8_t {
    Unknown = 0,
    Wifi = 1,
    Ble = 2,
    Ieee802154 = 3,
};

enum class Band : uint8_t {
    Unknown = 0,
    Band2_4GHz = 1,
    Band5GHz = 2,
};

// The detectors a classifier may report. A classifier evaluates a detector,
// and updates any state that detector keeps, only when it is in the set.
// Numbers that are not known detectors are never members, so a value read
// from storage or a radio link cannot switch on something that does not
// exist.
class DetectorSet {
public:
    constexpr DetectorSet() = default;

    static constexpr DetectorSet none() { return DetectorSet(); }

    static constexpr DetectorSet all()
    {
        DetectorSet s;
        for (DetectorId d : kAllDetectors) s.add(d);
        return s;
    }

    constexpr void add(DetectorId d)
    {
        const uint8_t v = static_cast<uint8_t>(d);
        if (isKnownDetector(v)) _bits |= (uint32_t{1} << v);
    }

    constexpr void remove(DetectorId d)
    {
        const uint8_t v = static_cast<uint8_t>(d);
        if (v < 32) _bits &= ~(uint32_t{1} << v);
    }

    constexpr bool contains(DetectorId d) const
    {
        const uint8_t v = static_cast<uint8_t>(d);
        return v < 32 && (_bits & (uint32_t{1} << v)) != 0;
    }

    constexpr bool empty() const { return _bits == 0; }

    constexpr bool operator==(const DetectorSet &other) const { return _bits == other._bits; }
    constexpr bool operator!=(const DetectorSet &other) const { return _bits != other._bits; }

private:
    uint32_t _bits = 0;
};

} // namespace lts
