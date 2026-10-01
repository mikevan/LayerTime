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

// LayerTime on the T-Watch S3 Plus, version 0.2.3 (Recon milestone R1).
//
// Everything is in app/S3PlusApp: LilyGoLib bring-up and the hardware report
// (BringUpReport), the core and the S3 Plus adapters behind its ports
// (recon/, gnss/, S3PlusAlertSink, S3PlusSettingsStore), and the screens
// (ui/). The LoRa radio is never initialised and its rail is switched off;
// mesh stays deferred. No Recon logging yet (R2).
//
// The S3 Plus lives in lilygo-s3plus/, outside src/, so the T-Watch Ultra build
// (which compiles everything under src/) never sees these files.

#include <Arduino.h>
#include <lvgl.h>

#include "app/S3PlusApp.h"
#include "S3PlusProfile.h"

#if !defined(LAYERTIME_S3PLUS_LV_CONF)
#error "LVGL is not using lilygo-s3plus/lv_conf_s3plus.h"
#endif

#if !defined(ARDUINO_T_WATCH_S3)
#error "The S3 Plus target must build LilyGoLib's T-Watch S3 board class"
#endif

namespace {
layertime::twatch_s3plus::S3PlusApp gApp;
}

void setup() { gApp.begin(); }

void loop() { gApp.loop(); }
