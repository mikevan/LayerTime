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

#pragma once

// AlertSink on the S3 Plus: wake the display and vibrate, as on the T-Ultra
// (TUltraAlertSink). The core decides whether to alert (sleep mode and Low
// confidence suppress it); this only decides how.

#include "core/ports/AlertSink.h"

namespace layertime {
namespace twatch_s3plus {

class S3PlusAlertSink : public AlertSink {
public:
    void raise(const Alert &alert) override;
};

} // namespace twatch_s3plus
} // namespace layertime
