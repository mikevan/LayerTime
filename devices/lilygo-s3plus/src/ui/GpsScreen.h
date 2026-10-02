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

// The S3 Plus GPS page: the T-Ultra's GpsScreen (src/platform/twatch_ultra/
// ui/GpsScreen.cpp) scaled from 410 x 502 to 240 x 240, in the Ultra's design
// language: the filled gold "< BACK" button, the green "GPS" title, the fix
// status, LAT and LON, the MGRS grid reference, a two-up grid of ALT, SATS,
// ACC, and SPD, the direction of travel, and the "LAYERTIME | GPS" footer.
//
// One deliberate difference: "ACC" (the receiver's own horizontal accuracy
// estimate) where the Ultra shows HDOP. This watch reads no NMEA, and HDOP is
// satellite geometry, not an error estimate.

#include <lvgl.h>
#include <stdint.h>

namespace layertime {
namespace twatch_s3plus {

class S3PlusApp;

class GpsScreen {
public:
    void create(S3PlusApp *app);
    void show();
    void render();

private:
    static void backThunk(lv_event_t *event);
    lv_obj_t *value(int32_t x, int32_t y, int32_t width, const lv_font_t *font, lv_color_t colour);

    S3PlusApp *_app = nullptr;
    lv_obj_t *_screen = nullptr;
    lv_obj_t *_status = nullptr;
    lv_obj_t *_latitude = nullptr;
    lv_obj_t *_longitude = nullptr;
    lv_obj_t *_mgrs = nullptr;
    lv_obj_t *_altitude = nullptr;
    lv_obj_t *_satellites = nullptr;
    lv_obj_t *_accuracy = nullptr;
    lv_obj_t *_speed = nullptr;
    lv_obj_t *_course = nullptr;
};

} // namespace twatch_s3plus
} // namespace layertime
