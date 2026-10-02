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

// The Phase 1 hardware report, kept from bring-up 0.1.3 and printed once at
// boot over USB serial: chip, MAC, PSRAM, both I2C buses against LilyGo's
// documented parts, the GNSS module, battery, RTC, and FFat (partition,
// mount, capacity, and the stored storage reboot-test result).
//
// What 0.2.0 dropped from bring-up: the one-time format control (it had run
// and was locked by its NVS record) and the "Run storage reboot test"
// button. A reboot test left pending by 0.1.3 is still completed at boot and
// its outcome stored, as before. Both are in the history of
// devices/lilygo-s3plus/src/S3PlusMain.cpp at 0.1.3.

namespace layertime {
namespace twatch_s3plus {
namespace bringup_report {

// Before LilyGoLib's begin(): it waits forever when there is no PSRAM.
void beforeBegin();
// After begin(): I2C scans, the GNSS model, storage, and any pending
// storage reboot test.
void afterBegin();
void print(const char *version);

bool psramOk();
bool mainBusOk();
bool touchBusOk();
bool storageOk();
const char *gnssName();

} // namespace bringup_report
} // namespace twatch_s3plus
} // namespace layertime
