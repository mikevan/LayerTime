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

#include "Signatures.h"

#include <stdio.h>
#include <string.h>

namespace lts {

bool isFindMyBeacon(const uint8_t *mfg, size_t length)
{
    if (mfg == nullptr || length < 3) return false;
    const uint8_t subtype = mfg[2];
    if (subtype == 0x12 || subtype == 0x1E) return true;
    return kMatchProximityPairing && subtype == 0x07;
}

bool allDigits(const std::string &value)
{
    if (value.empty()) return false;
    for (char c : value) if (c < '0' || c > '9') return false;
    return true;
}

bool isFlockName(const std::string &name)
{
    return name.empty() || name.rfind("Penguin-", 0) == 0 ||
           name == "FS Ext Battery" || (name.length() == 10 && allDigits(name));
}

void formatMac(char *out, size_t outSize, const uint8_t *mac)
{
    if (!out || outSize < 18 || !mac) return;
    snprintf(out, outSize, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

uint16_t hashSsid(const uint8_t *ssid, size_t length)
{
    uint16_t hash = 5381;
    for (size_t i = 0; i < length; ++i)
        hash = static_cast<uint16_t>(((hash << 5) + hash) + ssid[i]);
    return hash;
}

bool isPineappleOui(const uint8_t *mac, bool openNetwork)
{
    const uint32_t oui = (static_cast<uint32_t>(mac[0]) << 16) |
                         (static_cast<uint32_t>(mac[1]) << 8) | mac[2];
    switch (oui) {
    case 0x001337: case 0x02C0CA: case 0x021337: case 0x000A00:
    case 0x000C43: case 0x000CE7: case 0x0017A5: case 0x9CEFD5:
    case 0x9CE5D5: case 0xDEADBE: return true;
    case 0x00C0CA: case 0x1CBFCE: case 0x0CEFAF: return openNetwork;
    default: return false;
    }
}

const OuiSignature *lookupOui(const uint8_t *mac)
{
    if (mac == nullptr) return nullptr;
    for (const OuiSignature &sig : kOuiSignatures)
        if (memcmp(mac, sig.oui, 3) == 0) return &sig;
    return nullptr;
}

bool ssidHasPrefix(const uint8_t *ssid, uint8_t ssidLength, const char *prefix)
{
    const size_t n = strlen(prefix);
    if (ssidLength < n) return false;
    for (size_t i = 0; i < n; ++i) {
        char a = static_cast<char>(ssid[i]);
        char b = prefix[i];
        if (a >= 'a' && a <= 'z') a = static_cast<char>(a - 32);
        if (b >= 'a' && b <= 'z') b = static_cast<char>(b - 32);
        if (a != b) return false;
    }
    return true;
}

} // namespace lts
