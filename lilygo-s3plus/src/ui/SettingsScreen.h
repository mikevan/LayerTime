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

// The S3 Plus Settings screen: the T-Ultra's Settings page (src/platform/
// twatch_ultra/ui/SettingsScreen) row for row, laid out again for 240 x 240,
// in one scrolling list. Opened by a long press on the watch face, as on the
// Ultra and the Garmin.
//
//   DATE / TIME           opens the DATE / TIME page (TimeScreen)
//   BRIGHTNESS            slider, 20 to 255
//   CLOCK FORMAT          12 H / 24 H
//   UNITS                 IMPERIAL / METRIC
//   GPS                   ON / OFF (the receiver's power rail)
//   MESHCORE ... NAME     greyed, "LATER": mesh is a later milestone
//   EARLY WARNING         ON / OFF
//   LOGGING               greyed, "LATER" until logging is built (the Ultra's
//                         SD LOGGING; this watch has no SD card)
//   SLEEP MODE            ON / OFF: alerts neither buzz nor pop; everything
//                         is still logged
//   SQUACHIFY?            greyed, "LATER" until the art is in
//   DONGLE ICON           ON / OFF: the face's DONGLE icon, for wearers
//                         without a dongle (S3 Plus only)
//
// Intentional difference from the Ultra: no SD CARD row (no SD card), and a
// row with nothing behind it yet is greyed and reads "LATER" instead of
// offering a switch that would do nothing.
//
// Built once in create() and shown, never rebuilt (the Ultra's lesson).

#include <lvgl.h>
#include <stdint.h>

namespace layertime {
namespace twatch_s3plus {

class S3PlusApp;

class SettingsScreen {
public:
    void create(S3PlusApp *app);
    void show();
    void render();

private:
    lv_obj_t *row(const char *title, lv_event_cb_t onClick, lv_obj_t **value);
    lv_obj_t *laterRow(const char *title);
    void refreshValues();

    static void backThunk(lv_event_t *event);
    static void dateTimeThunk(lv_event_t *event);
    static void brightnessThunk(lv_event_t *event);
    static void brightnessReleasedThunk(lv_event_t *event);
    static void clockFormatThunk(lv_event_t *event);
    static void unitsThunk(lv_event_t *event);
    static void gpsThunk(lv_event_t *event);
    static void earlyWarningThunk(lv_event_t *event);
    static void sleepModeThunk(lv_event_t *event);
    static void dongleIconThunk(lv_event_t *event);

    S3PlusApp *_app = nullptr;
    lv_obj_t *_screen = nullptr;
    lv_obj_t *_page = nullptr;
    lv_obj_t *_brightnessValue = nullptr;
    lv_obj_t *_brightnessSlider = nullptr;
    lv_obj_t *_clockValue = nullptr;
    lv_obj_t *_unitsValue = nullptr;
    lv_obj_t *_gpsValue = nullptr;
    lv_obj_t *_earlyWarningValue = nullptr;
    lv_obj_t *_sleepModeValue = nullptr;
    lv_obj_t *_dongleIconValue = nullptr;
};

} // namespace twatch_s3plus
} // namespace layertime
