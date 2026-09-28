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

// The Recon detection log: where it goes, its header, and how one row is
// written. Moved unchanged out of WatchApp::logReconDetection in Phase 0
// Step 3i. Whether logging is on, and writing to the card, stay with the
// application and the platform.
//
// KNOWN DEFECT, carried over unchanged: fields are written as-is, without
// quoting or escaping, so a detail containing a comma or a quote shifts or
// breaks the columns. Pinned by test_detection_log:
// fields_are_not_quoted_or_escaped_KNOWN_DEFECT.

#include <stddef.h>

namespace layertime {
namespace detection_log {

extern const char *const kPath;
extern const char *const kHeader;

// Worst case is about 107 bytes with the confidence column; sized well clear
// of that so a long detail string truncates the field, never the row.
constexpr size_t kRowBufferSize = 160;

struct RowFields {
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    const char *category = "";
    const char *detail = "";
    const char *address = "";
    int rssi = 0;
    unsigned channel = 0;
    const char *confidence = "";
};

// Writes one row, without a line ending, into `out`. Same return value and
// truncation as snprintf.
int formatRow(const RowFields &fields, char *out, size_t outSize);

} // namespace detection_log
} // namespace layertime
