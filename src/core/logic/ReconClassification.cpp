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

#include "ReconClassification.h"

#include "ReconSelection.h"

namespace layertime {
namespace recon {

bool toSensorDetector(ReconTarget target, lts::DetectorId &out)
{
    switch (target) {
    case ReconTarget::Deauth: out = lts::DetectorId::Deauth; return true;
    case ReconTarget::Pwnagotchi: out = lts::DetectorId::Pwnagotchi; return true;
    case ReconTarget::MultiSSID: out = lts::DetectorId::MultiSSID; return true;
    case ReconTarget::Flock: out = lts::DetectorId::Flock; return true;
    case ReconTarget::Pineapple: out = lts::DetectorId::Pineapple; return true;
    case ReconTarget::AirTag: out = lts::DetectorId::AirTag; return true;
    case ReconTarget::Flipper: out = lts::DetectorId::Flipper; return true;
    case ReconTarget::Meta: out = lts::DetectorId::Meta; return true;
    case ReconTarget::Axon: out = lts::DetectorId::Axon; return true;
    case ReconTarget::Tile: out = lts::DetectorId::Tile; return true;
    case ReconTarget::SamsungTag: out = lts::DetectorId::SamsungTag; return true;
    case ReconTarget::GoogleTag: out = lts::DetectorId::GoogleTag; return true;
    default: return false;
    }
}

ReconTarget fromSensorDetector(lts::DetectorId detector)
{
    switch (detector) {
    case lts::DetectorId::Deauth: return ReconTarget::Deauth;
    case lts::DetectorId::Pwnagotchi: return ReconTarget::Pwnagotchi;
    case lts::DetectorId::MultiSSID: return ReconTarget::MultiSSID;
    case lts::DetectorId::Flock: return ReconTarget::Flock;
    case lts::DetectorId::Pineapple: return ReconTarget::Pineapple;
    case lts::DetectorId::AirTag: return ReconTarget::AirTag;
    case lts::DetectorId::Flipper: return ReconTarget::Flipper;
    case lts::DetectorId::Meta: return ReconTarget::Meta;
    case lts::DetectorId::Axon: return ReconTarget::Axon;
    case lts::DetectorId::Tile: return ReconTarget::Tile;
    case lts::DetectorId::SamsungTag: return ReconTarget::SamsungTag;
    case lts::DetectorId::GoogleTag: return ReconTarget::GoogleTag;
    }
    return ReconTarget::None;
}

lts::DetectorSet bleScanDetectors(ReconTarget scanSelection)
{
    lts::DetectorSet set;
    for (lts::DetectorId d : lts::kAllDetectors)
        if (bleScanWants(scanSelection, fromSensorDetector(d))) set.add(d);
    return set;
}

lts::DetectorSet enabledDetectors(WantsFn wants, const void *wantsContext)
{
    lts::DetectorSet set;
    for (lts::DetectorId d : lts::kAllDetectors)
        if (wants(fromSensorDetector(d), wantsContext)) set.add(d);
    return set;
}

namespace {

struct Forward {
    CandidateSink sink;
    void *context;
};

// lts::Candidate -> core Candidate. The radio, band, and time stay at their
// defaults; the device stamps them, as it always has.
void forward(const lts::Candidate &in, void *context)
{
    const Forward *f = static_cast<const Forward *>(context);
    Candidate c;
    c.detector = fromSensorDetector(in.detector);
    c.detail = in.detail;
    c.address = in.address;
    c.rssi = in.rssi;
    c.confidence = in.confidence;
    c.channel = in.channel;
    f->sink(c, f->context);
}

} // namespace

void classifyBleAdvert(const BleAdvertSource &advert, ReconTarget scanSelection,
                       CandidateSink sink, void *sinkContext)
{
    Forward f{sink, sinkContext};
    lts::classifyBleAdvert(advert, bleScanDetectors(scanSelection), forward, &f);
}

void WifiFrameClassifier::reset()
{
    _sensor.reset();
}

void WifiFrameClassifier::classify(const uint8_t *frame, uint16_t length, int8_t rssi,
                                   uint8_t channel, uint32_t nowMs, WantsFn wants,
                                   const void *wantsContext, CandidateSink sink,
                                   void *sinkContext)
{
    Forward f{sink, sinkContext};
    _sensor.classify(frame, length, rssi, channel, nowMs, enabledDetectors(wants, wantsContext),
                     forward, &f);
}

} // namespace recon
} // namespace layertime
