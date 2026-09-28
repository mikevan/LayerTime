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

#include "ReconSelection.h"

namespace layertime {
namespace recon {

namespace {
constexpr ReconTarget kTrackerMembers[] = {
    ReconTarget::AirTag, ReconTarget::Tile, ReconTarget::SamsungTag,
    ReconTarget::GoogleTag};
constexpr ReconTarget kCounterSurveilMembers[] = {
    ReconTarget::Flock, ReconTarget::Axon, ReconTarget::Meta};
constexpr ReconTarget kCounterIntrusionMembers[] = {
    ReconTarget::Deauth, ReconTarget::Pwnagotchi, ReconTarget::MultiSSID,
    ReconTarget::Pineapple, ReconTarget::Flipper};
}

const ReconTarget *groupMembers(ReconTarget group, size_t &count)
{
    switch (group) {
    case ReconTarget::Trackers:
        count = sizeof(kTrackerMembers) / sizeof(kTrackerMembers[0]);
        return kTrackerMembers;
    case ReconTarget::CounterSurveil:
        count = sizeof(kCounterSurveilMembers) / sizeof(kCounterSurveilMembers[0]);
        return kCounterSurveilMembers;
    case ReconTarget::CounterIntrusion:
        count = sizeof(kCounterIntrusionMembers) / sizeof(kCounterIntrusionMembers[0]);
        return kCounterIntrusionMembers;
    default:
        count = 0;
        return nullptr;
    }
}

bool groupContains(ReconTarget group, ReconTarget detector)
{
    size_t count = 0;
    const ReconTarget *members = groupMembers(group, count);
    for (size_t i = 0; i < count; ++i)
        if (members[i] == detector) return true;
    return false;
}

bool isBleDetector(ReconTarget d)
{
    return d == ReconTarget::Flock || d == ReconTarget::AirTag ||
           d == ReconTarget::Flipper || d == ReconTarget::Meta ||
           d == ReconTarget::Tile || d == ReconTarget::SamsungTag ||
           d == ReconTarget::GoogleTag;
}

bool isWifiDetector(ReconTarget d)
{
    return d == ReconTarget::Deauth || d == ReconTarget::Pwnagotchi ||
           d == ReconTarget::MultiSSID || d == ReconTarget::Pineapple ||
           d == ReconTarget::Flock || d == ReconTarget::Axon;
}

bool needsBle(ReconTarget selection)
{
    if (selection == ReconTarget::All) return true;
    size_t count = 0;
    const ReconTarget *members = groupMembers(selection, count);
    if (members == nullptr) return isBleDetector(selection);
    for (size_t i = 0; i < count; ++i)
        if (isBleDetector(members[i])) return true;
    return false;
}

bool needsWifi(ReconTarget selection)
{
    if (selection == ReconTarget::All) return true;
    size_t count = 0;
    const ReconTarget *members = groupMembers(selection, count);
    if (members == nullptr) return isWifiDetector(selection);
    for (size_t i = 0; i < count; ++i)
        if (isWifiDetector(members[i])) return true;
    return false;
}

bool bleScanWants(ReconTarget scanSelection, ReconTarget target)
{
    if (scanSelection == ReconTarget::All) return true;
    if (scanSelection == ReconTarget::EarlyWarning)
        return target == ReconTarget::Flipper || target == ReconTarget::Meta;
    if (scanSelection == target) return true;
    return groupContains(scanSelection, target);
}

bool isBackgroundWifiDetector(ReconTarget detector)
{
    switch (detector) {
    case ReconTarget::Deauth:
    case ReconTarget::Pwnagotchi:
    case ReconTarget::Pineapple:
    case ReconTarget::MultiSSID:
        return true;
    default:
        return false;
    }
}

bool wants(bool monitoring, ReconTarget selection, bool earlyWarningSweeping,
           ReconTarget detector)
{
    if (monitoring) {
        return selection == ReconTarget::All || selection == detector ||
               groupContains(selection, detector);
    }
    return earlyWarningSweeping && isBackgroundWifiDetector(detector);
}

const char *detectorName(ReconTarget detector)
{
    switch (detector) {
    case ReconTarget::All: return "ALL";
    case ReconTarget::Trackers: return "TRACKERS";
    case ReconTarget::CounterSurveil: return "COUNTER-SURVEIL";
    case ReconTarget::CounterIntrusion: return "COUNTER-INTRUSION";
    case ReconTarget::Deauth: return "DEAUTH";
    case ReconTarget::Pwnagotchi: return "PWNAGOTCHI";
    case ReconTarget::MultiSSID: return "MULTISSID";
    case ReconTarget::Flock: return "FLOCK";
    case ReconTarget::Pineapple: return "PINEAPPLE";
    case ReconTarget::AirTag: return "AIRTAG";
    case ReconTarget::Flipper: return "FLIPPER";
    case ReconTarget::Meta: return "META";
    case ReconTarget::Axon: return "AXON";
    case ReconTarget::Tile: return "TILE";
    case ReconTarget::SamsungTag: return "SMARTTAG";
    case ReconTarget::GoogleTag: return "GOOGLE TAG";
    case ReconTarget::EarlyWarning: return "EARLY WARNING";
    default: return "STOPPED";
    }
}

const char *detectorShortName(ReconTarget detector)
{
    switch (detector) {
    case ReconTarget::CounterSurveil: return "SURVEIL";
    case ReconTarget::CounterIntrusion: return "INTRUSION";
    case ReconTarget::EarlyWarning: return "EARLY WARN";
    default: return detectorName(detector);
    }
}

const char *confidenceLabel(Confidence confidence)
{
    switch (confidence) {
    case Confidence::High: return "HIGH";
    case Confidence::Medium: return "MED";
    default: return "LOW";
    }
}

} // namespace recon
} // namespace layertime
