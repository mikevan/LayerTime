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

#include "SdCardService.h"

#include <SD.h>
#include <SPI.h>

namespace {
constexpr uint32_t kSpiFrequency = 4000000U;
constexpr const char *kMountPoint = "/sd";
constexpr uint8_t kMaxOpenFiles = 5;
// Sectors zeroed to force-invalidate an already-mounted filesystem before a
// destructive reformat (boot sector + backup boot sector/FSInfo region on
// FAT32 - zeroed generously to be safe rather than exact).
constexpr uint32_t kSectorsToWipe = 8;
}

void SdCardService::begin()
{
    // LilyGoLib's own board bring-up (instance.begin() -> installSD()) has
    // already attempted SD.begin() with these exact pins/mountpoint by the
    // time this runs - just read the result instead of mounting again.
    _status = (SD.cardType() != CARD_NONE) ? SdCardStatus::Ready : SdCardStatus::NotRecognized;
}

bool SdCardService::refresh()
{
    return mountInternal(false);
}

bool SdCardService::formatAndMount()
{
    if (_status == SdCardStatus::Ready) {
        // format-on-mount-failure (below) only triggers when the mount
        // actually fails, which it won't for a card that's already fine.
        // Force that by invalidating the current filesystem's boot sector
        // region first, while the card is still initialized and raw sector
        // access is valid.
        uint8_t zero[512] = {0};
        for (uint32_t sector = 0; sector < kSectorsToWipe; ++sector) {
            SD.writeRAW(zero, sector);
        }
    }
    return mountInternal(true);
}

bool SdCardService::mountInternal(bool formatIfEmpty)
{
    SD.end();
    const bool mounted = SD.begin(SD_CS, SPI, kSpiFrequency, kMountPoint, kMaxOpenFiles, formatIfEmpty);
    _status = (mounted && SD.cardType() != CARD_NONE) ? SdCardStatus::Ready : SdCardStatus::NotRecognized;
    return _status == SdCardStatus::Ready;
}

uint64_t SdCardService::totalBytes() const
{
    return _status == SdCardStatus::Ready ? SD.totalBytes() : 0;
}

uint64_t SdCardService::usedBytes() const
{
    return _status == SdCardStatus::Ready ? SD.usedBytes() : 0;
}

bool SdCardService::appendCsvRow(const char *path, const char *header, const char *row)
{
    if (_status != SdCardStatus::Ready || path == nullptr || row == nullptr) {
        return false;
    }

    const bool isNewFile = !SD.exists(path);
    File file = SD.open(path, FILE_APPEND);
    if (!file) {
        return false;
    }

    if (isNewFile && header != nullptr) {
        file.println(header);
    }
    file.println(row);
    file.close();
    return true;
}
