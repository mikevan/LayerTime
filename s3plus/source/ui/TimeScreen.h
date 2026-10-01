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

// The S3 Plus DATE / TIME page, opened from Settings: the T-Ultra's DATE /
// TIME page (SettingsScreen's date and time set: DAY, MONTH, YEAR, HOUR, MIN
// "-" and "+", CANCEL, SAVE) laid out again for 240 x 240, plus what only
// this watch has:
//
//   * TIME ZONE "-" and "+" (15-minute steps, UTC-12:00 to UTC+14:00). The
//     clock is set from GNSS UTC plus this offset, so it changes the time at
//     once, as it did on the R1 Time page.
//   * A status line: when GNSS last set the clock, or that it was set by
//     hand, or that it is waiting for GNSS.
//
// SAVE writes the edited date and time to the clock; GNSS still wins at its
// next set (S3PlusClock). The 12-hour or 24-hour choice is CLOCK FORMAT on
// Settings, as on the Ultra. Stepping follows gnss::stepDateField.

#include <lvgl.h>
#include <stdint.h>

#include "../gnss/GnssRules.h"

namespace layertime {
namespace twatch_s3plus {

class S3PlusApp;

class TimeScreen {
public:
    void create(S3PlusApp *app);
    // Opens with the edit set to the clock's current local time.
    void show();
    void render(uint32_t nowMs);

private:
    struct Step {
        TimeScreen *screen = nullptr;
        gnss::DateField field = gnss::DateField::Day;
        int direction = 0;
    };

    struct Zone {
        TimeScreen *screen = nullptr;
        int direction = 0;
    };

    lv_obj_t *stepButton(lv_obj_t *parent, const char *text, int32_t x, int32_t y, int32_t width,
                         gnss::DateField field, int direction, size_t &index);
    void refreshEdit();

    static void backThunk(lv_event_t *event);
    static void zoneThunk(lv_event_t *event);
    static void stepThunk(lv_event_t *event);
    static void saveThunk(lv_event_t *event);

    S3PlusApp *_app = nullptr;
    lv_obj_t *_screen = nullptr;
    lv_obj_t *_status = nullptr;
    lv_obj_t *_offset = nullptr;
    lv_obj_t *_editDate = nullptr;
    lv_obj_t *_editTime = nullptr;
    gnss::DateTime _edit;
    Step _steps[10];
    Zone _zones[2];
};

} // namespace twatch_s3plus
} // namespace layertime
