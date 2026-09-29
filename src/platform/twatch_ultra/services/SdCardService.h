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

#include <stdint.h>

// Owns the microSD card lifecycle for LayerTime: mount, destructive format
// (both "card unreadable" recovery and "wipe a working card on demand"),
// status/space reporting, and simple CSV logging.
//
// Built entirely on the standard Arduino SD.h API (the same one LilyGoLib's
// own board bring-up already uses successfully on this hardware: pin
// SD_CS=21, shared SPI bus, mount point "/sd") rather than talking to the
// underlying ESP-IDF SD/SPI driver directly - that keeps this from fighting
// LilyGoLib over the SPI bus it already initializes at boot.
enum class SdCardStatus : uint8_t {
    // begin()/refresh() hasn't run yet.
    Unknown,
    // No card responded, or the card doesn't contain a filesystem SD.begin()
    // recognizes (e.g. left in a state by a Linux-based tool like a
    // Pwnagotchi/Ragnar). This is the "offer to format" state.
    NotRecognized,
    // Mounted at /sd and ready for file I/O.
    Ready,
};

class SdCardService {
public:
    // Call once after instance.begin(). LilyGoLib's own board bring-up
    // already attempts a mount at boot (installSD(), same SD.begin() call);
    // this just reads the result rather than mounting a second time.
    void begin();

    // Re-probe the card non-destructively (unmounts first if needed, then
    // attempts a normal mount). Returns true if the card ends up Ready.
    bool refresh();

    // Destructive: wipes the card and leaves it freshly formatted FAT32 and
    // mounted. Works from either starting state -
    //   - NotRecognized: mounts with format-on-failure, which formats
    //     because the existing filesystem isn't recognized.
    //   - Ready: the card mounts fine as-is, so format-on-failure alone
    //     wouldn't trigger. To force a wipe anyway, the first few sectors
    //     (boot sector + backup/FSInfo region) are zeroed via the SD
    //     library's raw sector write first, which invalidates whatever
    //     filesystem was there; the card is then unmounted and remounted
    //     with format-on-failure, which now formats it for real.
    // Returns true if the card ends up Ready afterward.
    bool formatAndMount();

    SdCardStatus status() const { return _status; }
    uint64_t totalBytes() const;
    uint64_t usedBytes() const;

    // Appends one CSV row to `path` (relative to the /sd mount, e.g.
    // "/recon_log.csv"). Writes `header` first if the file doesn't exist
    // yet. No-ops (returns false) unless status() == Ready. Not CSV-escaped:
    // fine for our fixed detector labels/addresses, not general-purpose.
    bool appendCsvRow(const char *path, const char *header, const char *row);

private:
    bool mountInternal(bool formatIfEmpty);

    SdCardStatus _status = SdCardStatus::Unknown;
};
