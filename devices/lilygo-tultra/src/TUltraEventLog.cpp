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

#include "TUltraEventLog.h"

#include "core/logic/DetectionCsv.h"
#include "core/logic/ReconSelection.h"

namespace layertime {
namespace twatch_ultra {

void TUltraEventLog::append(const MonitorEvent &event)
{
    if (!_settings.reconSdLoggingEnabled) {
        return;
    }

    // The row format lives in core (src/core/logic/DetectionCsv).
    detection_log::RowFields fields;
    fields.year = _state.year;
    fields.month = _state.month;
    fields.day = _state.day;
    fields.hour = _state.hour;
    fields.minute = _state.minute;
    fields.second = _state.second;
    // The category column is the detector's display name, as it was when
    // the record carried it as text.
    fields.category = recon::detectorName(event.detector);
    fields.detail = event.detail;
    fields.address = event.sourceId;
    fields.rssi = event.rssi;
    fields.channel = static_cast<unsigned>(event.channel);
    fields.confidence = recon::confidenceLabel(event.confidence);

    char row[detection_log::kRowBufferSize];
    detection_log::formatRow(fields, row, sizeof(row));

    _sdCard.appendCsvRow(detection_log::kPath, detection_log::kHeader, row);
}

} // namespace twatch_ultra
} // namespace layertime
