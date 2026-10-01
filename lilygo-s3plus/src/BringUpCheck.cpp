// LayerTime - counter-intrusion and resilient-communications firmware
// for the LilyGo T-Watch S3 Plus.
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

#include "BringUpCheck.h"

#include <string.h>

namespace layertime {
namespace twatch_s3plus {
namespace bringup {

GnssModule classifyGnss(const char *model)
{
    if (model == nullptr || model[0] == '\0') return GnssModule::None;
    if (strstr(model, "MIA-M10") != nullptr) return GnssModule::MiaM10Q;
    if (strstr(model, "LS550G") != nullptr) return GnssModule::Ls550g;
    return GnssModule::Other;
}

const char *gnssName(GnssModule module)
{
    switch (module) {
    case GnssModule::None: return "none answered";
    case GnssModule::MiaM10Q: return "u-blox MIA-M10Q";
    case GnssModule::Ls550g: return "Quectel LS550G";
    case GnssModule::Other: return "unrecognised module";
    }
    return "unrecognised module";
}

bool psramAsExpected(bool found, uint32_t psramSize)
{
    return found && psramSize > (kExpectedPsramBytes / 16u) * 15u && psramSize <= kExpectedPsramBytes;
}

bool ffatPartitionIsExpected(const char *label, uint32_t address, uint32_t size,
                             unsigned fatPartitionCount)
{
    return label != nullptr && strcmp(label, kFfatLabel) == 0 && address == kFfatAddress &&
           size == kFfatSize && fatPartitionCount == 1;
}

HoldGate::Event HoldGate::update(bool pressed, uint32_t nowMs)
{
    if (_fired) return Event::None;
    if (!_holding) {
        if (!pressed) return Event::None;
        _holding = true;
        _startMs = nowMs;
        return Event::Started;
    }
    if (!pressed) {
        _holding = false;
        return Event::Cancelled;
    }
    if (nowMs - _startMs >= _holdMs) {
        _holding = false;
        _fired = true;
        return Event::Fired;
    }
    return Event::None;
}

uint8_t HoldGate::percent(uint32_t nowMs) const
{
    if (!_holding || _holdMs == 0) return 0;
    const uint32_t held = nowMs - _startMs;
    return held >= _holdMs ? 100 : static_cast<uint8_t>(held * 100u / _holdMs);
}

RebootTestResult classifyRebootTest(bool mounted, bool existed, bool matched, bool removed)
{
    if (!mounted) return RebootTestResult::NotMounted;
    if (!existed) return RebootTestResult::FileMissing;
    if (!matched) return RebootTestResult::Mismatch;
    if (!removed) return RebootTestResult::NotRemoved;
    return RebootTestResult::Pass;
}

const char *rebootTestText(RebootTestResult result)
{
    switch (result) {
    case RebootTestResult::None: return "No storage reboot test has completed.";
    case RebootTestResult::Pass:
        return "PASS. The test file survived the reboot, matched, and was removed.";
    case RebootTestResult::NotMounted: return "FAIL. Storage was not mounted after the reboot.";
    case RebootTestResult::FileMissing: return "FAIL. The test file did not survive the reboot.";
    case RebootTestResult::Mismatch: return "FAIL. The test file survived but its contents changed.";
    case RebootTestResult::NotRemoved:
        return "FAIL. The test file matched but could not be removed.";
    case RebootTestResult::WriteFailed:
        return "FAIL. The test file could not be written and read back.";
    }
    return "No storage reboot test has completed.";
}

RebootTestResult rebootTestFromStored(uint8_t stored)
{
    return stored <= static_cast<uint8_t>(RebootTestResult::WriteFailed)
               ? static_cast<RebootTestResult>(stored)
               : RebootTestResult::None;
}

} // namespace bringup
} // namespace twatch_s3plus
} // namespace layertime
