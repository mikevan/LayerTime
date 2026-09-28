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

#include "BleAdvertClassifier.h"

#include "ReconSelection.h"
#include "ReconSignatures.h"

namespace layertime {
namespace recon {

namespace {
void emit(CandidateSink sink, void *context, ReconTarget detector, const char *detail,
          const char *address, int8_t rssi, Confidence confidence)
{
    Candidate c;
    c.detector = detector;
    c.detail = detail;
    c.address = address;
    c.rssi = rssi;
    c.confidence = confidence;
    c.channel = 0;  // BLE: no Wi-Fi channel
    sink(c, context);
}
}

void classifyBleAdvert(const BleAdvertSource &advert, ReconTarget scan, CandidateSink sink,
                       void *sinkContext)
{
    const std::string name(advert.name, advert.nameLength);
    const char *address = advert.printedAddress;
    const int8_t rssi = advert.rssi;
    const char *label = name.empty() ? nullptr : name.c_str();

    // ---- manufacturer data ----
    // An advertisement may carry more than one manufacturer record, so walk
    // them all rather than assuming index 0.
    std::string mfg;
    for (uint8_t i = 0; i < advert.manufacturerCount; ++i) {
        advert.manufacturer(i, mfg, advert.context);
        if (mfg.size() < 2) continue;
        const uint8_t *bytes = reinterpret_cast<const uint8_t *>(mfg.data());
        const uint16_t company = static_cast<uint16_t>(bytes[0]) |
                                 static_cast<uint16_t>(static_cast<uint16_t>(bytes[1]) << 8);

        if (company == kAppleCompanyId && bleScanWants(scan, ReconTarget::AirTag) &&
            isFindMyBeacon(bytes, mfg.size())) {
            // High: Apple company ID plus a Find My offline-finding subtype.
            emit(sink, sinkContext, ReconTarget::AirTag, "Find My tracker beacon", address, rssi,
                 Confidence::High);
        }
        if (company == kXuntongCompanyId && bleScanWants(scan, ReconTarget::Flock) &&
            isFlockName(name)) {
            // High: the XUNTONG company ID alone would be weak, but it is
            // gated on a Flock-shaped device name.
            emit(sink, sinkContext, ReconTarget::Flock, label ? label : "Flock BLE signature",
                 address, rssi, Confidence::High);
        }
    }

    // Vendor OUI on the BLE address, independent of any advertised data.
    const OuiSignature *ouiMatch = lookupOui(advert.addressBytes);
    if (ouiMatch != nullptr && bleScanWants(scan, ouiMatch->detector)) {
        emit(sink, sinkContext, ouiMatch->detector, label ? label : ouiMatch->label, address,
             rssi, ouiMatch->confidence);
    }

    // ---- 16-bit service UUIDs ----
    for (uint8_t i = 0; i < advert.uuidCount; ++i) {
        uint16_t uuid = 0;
        if (!advert.uuid16(i, uuid, advert.context)) continue;
        for (const BleUuidSignature &sig : kBleUuidSignatures) {
            if (sig.uuid != uuid) continue;
            if (!bleScanWants(scan, sig.detector)) break;
            emit(sink, sinkContext, sig.detector, label ? label : sig.label, address, rssi,
                 sig.confidence);
            break;
        }
    }
}

} // namespace recon
} // namespace layertime
