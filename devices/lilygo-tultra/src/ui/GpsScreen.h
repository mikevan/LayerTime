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

#include <lvgl.h>

#include "core/model/Settings.h"
#include "../model/WatchState.h"

class GpsScreen {
public:
    using BackCallback = void (*)(void *userData);

    void create(BackCallback backCallback, void *userData);
    // Units come from the core's application settings since Phase 0 Step 6.
    void show(const WatchState &state, const layertime::ApplicationSettings &settings);
    void render(const WatchState &state, const layertime::ApplicationSettings &settings);
    lv_obj_t *screen() const { return _screen; }

private:
    static void backThunk(lv_event_t *event);

    lv_obj_t *makeValueLabel(
        int x,
        int y,
        int width,
        const lv_font_t *font,
        lv_color_t color,
        lv_text_align_t align = LV_TEXT_ALIGN_LEFT);

    lv_obj_t *_screen = nullptr;
    lv_obj_t *_status = nullptr;
    lv_obj_t *_latitude = nullptr;
    lv_obj_t *_longitude = nullptr;
    lv_obj_t *_altitude = nullptr;
    lv_obj_t *_satellites = nullptr;
    lv_obj_t *_hdop = nullptr;
    lv_obj_t *_speed = nullptr;
    lv_obj_t *_course = nullptr;
    lv_obj_t *_utm = nullptr;

    BackCallback _backCallback = nullptr;
    void *_userData = nullptr;
};
