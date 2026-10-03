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

// The S3 Plus watch face, after the Garmin face's design (devices/garmin-tactix/source/
// HomeView.mc), laid out for 240 x 240:
//
//   * battery dots across the top (24, lit left to right; red at 20% or
//     below) over "BAT 87%";
//   * the owl, large, in the middle, with a GPS icon and a DONGLE icon beside
//     its ears like a status bar: green when connected, red when not. The
//     DONGLE icon can be hidden in Settings;
//   * the time and the date, flanked by two rings as on the Garmin: WATCH
//     (the watch's own temperature, red when it runs hot) and the next solar
//     event (SUNRISE or SUNSET);
//   * "LOCATION" and "RECON";
//   * the Garmin's Recon bracket: the mode line ("EARLY WARN  REST", "RECON
//     ALL", "RECON OFF") over the detection count, amber, red on an alert.
//
// Taps: the bracket opens the running scan's monitor (or ALL), as THREATS
// did; the GPS icon and "LOCATION" open the GPS page; "RECON" the Recon
// menu. A long press anywhere on the face opens Settings, as on the Ultra
// and the Garmin. A double-tap on the face (anywhere but a button, the GPS
// icon, or the bracket) puts the display to sleep, as on the Ultra.

#include <lvgl.h>
#include <stdint.h>

#include "DisplayGate.h"
#include "OwlLogo.h"

namespace layertime {
namespace twatch_s3plus {

class S3PlusApp;

class HomeScreen {
public:
    static constexpr int kBatteryDots = 24;

    void create(S3PlusApp *app);
    void show();
    void render(uint32_t nowMs);

private:
    struct Ring {
        lv_obj_t *frame = nullptr;
        lv_obj_t *title = nullptr;
        lv_obj_t *value = nullptr;
    };
    struct Icon {
        lv_obj_t *box = nullptr;
        lv_obj_t *glyph = nullptr;
        lv_obj_t *label = nullptr;
    };

    Ring ring(int32_t x, int32_t y, lv_color_t colour, const char *title);
    Icon icon(int32_t x, int32_t y, const char *glyph, const char *label);
    void setIcon(const Icon &icon, bool connected);
    void buildBracket();

    static void reconThunk(lv_event_t *event);
    static void threatsThunk(lv_event_t *event);
    static void gpsThunk(lv_event_t *event);
    static void longPressThunk(lv_event_t *event);
    static void backgroundTapThunk(lv_event_t *event);

    S3PlusApp *_app = nullptr;
    lv_obj_t *_screen = nullptr;
    OwlLogo _owl;
    ui::DoubleTap _doubleTap;
    lv_obj_t *_dots[kBatteryDots] = {};
    lv_obj_t *_battery = nullptr;
    Ring _temperature;
    Ring _solar;
    Icon _gps;
    Icon _dongle;
    lv_obj_t *_time = nullptr;
    lv_obj_t *_date = nullptr;
    lv_obj_t *_bracketLines[8] = {};
    lv_point_precise_t _bracketPoints[8][2] = {};
    lv_obj_t *_mode = nullptr;
    lv_obj_t *_count = nullptr;
};

} // namespace twatch_s3plus
} // namespace layertime
