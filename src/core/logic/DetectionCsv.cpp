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

#include "DetectionCsv.h"

#include <stdio.h>

namespace layertime {
namespace detection_log {

const char *const kPath = "/recon_log.csv";
const char *const kHeader = "timestamp,category,detail,address,rssi,channel,confidence";

int formatRow(const RowFields &f, char *out, size_t outSize)
{
    return snprintf(
        out,
        outSize,
        "%04d-%02d-%02d %02d:%02d:%02d,%s,%s,%s,%d,%u,%s",
        f.year,
        f.month,
        f.day,
        f.hour,
        f.minute,
        f.second,
        f.category,
        f.detail,
        f.address,
        f.rssi,
        f.channel,
        f.confidence);
}

} // namespace detection_log
} // namespace layertime
