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

#include "TimeScreen.h"

#include <stdio.h>

#include "../app/S3PlusApp.h"
#include "S3Theme.h"
#include "TextFormat.h"

namespace layertime {
namespace twatch_s3plus {

namespace {
constexpr int32_t kTop = 34;
const char *const kMonths[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                 "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

lv_obj_t *label(lv_obj_t *parent, int32_t x, int32_t y, int32_t width, const lv_font_t *font,
                lv_color_t colour, const char *text)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_width(l, width);
    lv_obj_set_pos(l, x, y);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(l, colour, 0);
    lv_obj_set_style_text_font(l, font, 0);
    lv_label_set_text(l, text);
    return l;
}
} // namespace

void TimeScreen::create(S3PlusApp *app)
{
    _app = app;
    _screen = theme::screen();

    lv_obj_t *back = theme::button(_screen, "BACK", theme::gold(), &lv_font_montserrat_12, 56, 26);
    lv_obj_set_pos(back, 4, 4);
    lv_obj_add_event_cb(back, backThunk, LV_EVENT_CLICKED, this);
    label(_screen, 64, 9, 172, &lv_font_montserrat_14, theme::gold(), "DATE / TIME");

    // Everything below the header scrolls: this page is taller than the
    // screen.
    lv_obj_t *page = lv_obj_create(_screen);
    lv_obj_set_size(page, 240, 240 - kTop);
    lv_obj_set_pos(page, 0, kTop);
    lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_style_pad_all(page, 0, 0);
    lv_obj_set_scroll_dir(page, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(page, LV_SCROLLBAR_MODE_AUTO);

    _status = label(page, 6, 2, 228, &lv_font_montserrat_12, theme::muted(), "");
    lv_label_set_long_mode(_status, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(_status, LV_TEXT_ALIGN_LEFT, 0);

    // TIME ZONE: applied at once, as on the R1 Time page.
    label(page, 6, 40, 228, &lv_font_montserrat_12, theme::white(), "TIME ZONE");
    lv_obj_t *minus = theme::button(page, "-", theme::teal(), &lv_font_montserrat_20, 48, 30);
    lv_obj_set_pos(minus, 6, 58);
    _zones[0] = {this, -1};
    _zones[1] = {this, 1};
    lv_obj_add_event_cb(minus, zoneThunk, LV_EVENT_CLICKED, &_zones[0]);
    _offset = label(page, 56, 64, 128, &lv_font_montserrat_16, theme::gold(), "UTC+00:00");
    lv_obj_t *plus = theme::button(page, "+", theme::teal(), &lv_font_montserrat_20, 48, 30);
    lv_obj_set_pos(plus, 186, 58);
    lv_obj_add_event_cb(plus, zoneThunk, LV_EVENT_CLICKED, &_zones[1]);

    // The Ultra's date and time set.
    _editDate = label(page, 6, 102, 228, &lv_font_montserrat_20, theme::teal(), "--- -- ----");
    size_t index = 0;
    stepButton(page, "DAY -", 6, 130, 76, gnss::DateField::Day, -1, index);
    stepButton(page, "DAY +", 158, 130, 76, gnss::DateField::Day, 1, index);
    stepButton(page, "MONTH -", 6, 166, 76, gnss::DateField::Month, -1, index);
    stepButton(page, "MONTH +", 158, 166, 76, gnss::DateField::Month, 1, index);
    stepButton(page, "YEAR -", 6, 202, 76, gnss::DateField::Year, -1, index);
    stepButton(page, "YEAR +", 158, 202, 76, gnss::DateField::Year, 1, index);
    _editTime = label(page, 6, 240, 228, &lv_font_montserrat_28, theme::gold(), "--:--");
    stepButton(page, "HOUR -", 6, 278, 76, gnss::DateField::Hour, -1, index);
    stepButton(page, "HOUR +", 158, 278, 76, gnss::DateField::Hour, 1, index);
    stepButton(page, "MIN -", 6, 314, 76, gnss::DateField::Minute, -1, index);
    stepButton(page, "MIN +", 158, 314, 76, gnss::DateField::Minute, 1, index);

    lv_obj_t *cancel = theme::button(page, "CANCEL", theme::muted(), &lv_font_montserrat_14, 108, 32);
    lv_obj_set_pos(cancel, 6, 358);
    lv_obj_add_event_cb(cancel, backThunk, LV_EVENT_CLICKED, this);
    lv_obj_t *save = theme::button(page, "SAVE", theme::gold(), &lv_font_montserrat_14, 108, 32);
    lv_obj_set_pos(save, 126, 358);
    lv_obj_add_event_cb(save, saveThunk, LV_EVENT_CLICKED, this);
    // Room to scroll SAVE clear of the bottom edge.
    lv_obj_t *spacer = lv_obj_create(page);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_size(spacer, 1, 8);
    lv_obj_set_pos(spacer, 0, 392);
}

lv_obj_t *TimeScreen::stepButton(lv_obj_t *parent, const char *text, int32_t x, int32_t y, int32_t width,
                                 gnss::DateField field, int direction, size_t &index)
{
    lv_obj_t *b = theme::button(parent, text, theme::teal(), &lv_font_montserrat_12, width, 30);
    lv_obj_set_pos(b, x, y);
    if (index < sizeof(_steps) / sizeof(_steps[0])) {
        _steps[index] = {this, field, direction};
        lv_obj_add_event_cb(b, stepThunk, LV_EVENT_CLICKED, &_steps[index]);
        ++index;
    }
    return b;
}

void TimeScreen::show()
{
    if (_app) {
        // A zero step brings whatever the RTC holds into range.
        _edit = gnss::stepDateField(_app->localTime(), gnss::DateField::Day, 0);
        refreshEdit();
    }
    lv_screen_load(_screen);
}

void TimeScreen::refreshEdit()
{
    lv_label_set_text_fmt(_editDate, "%s %02d %04d", kMonths[_edit.month - 1], _edit.day, _edit.year);
    lv_label_set_text_fmt(_editTime, "%02d:%02d", _edit.hour, _edit.minute);
}

void TimeScreen::render(uint32_t nowMs)
{
    if (!_app || lv_screen_active() != _screen) return;
    char buf[96];
    gnss::formatOffset(_app->timeZoneMinutes(), buf, sizeof(buf));
    lv_label_set_text(_offset, buf);
    if (_app->clockSetThisBoot()) {
        char age[16];
        text::formatAge(nowMs - _app->clockLastSetMs(), age, sizeof(age));
        snprintf(buf, sizeof(buf), "GNSS set the clock %s ago. GNSS overrides a time saved here.", age);
    } else if (_app->clockSetManually()) {
        snprintf(buf, sizeof(buf), "Set by hand. GNSS will correct it once it has the time.");
    } else {
        snprintf(buf, sizeof(buf), "Waiting for GNSS time. Set the date and time here if there is no GNSS.");
    }
    lv_label_set_text(_status, buf);
}

void TimeScreen::backThunk(lv_event_t *event)
{
    auto *self = static_cast<TimeScreen *>(lv_event_get_user_data(event));
    if (self && self->_app) self->_app->showSettings();
}

void TimeScreen::zoneThunk(lv_event_t *event)
{
    auto *zone = static_cast<Zone *>(lv_event_get_user_data(event));
    if (!zone || !zone->screen || !zone->screen->_app) return;
    TimeScreen *self = zone->screen;
    const int before = self->_app->timeZoneMinutes();
    self->_app->stepTimeZone(zone->direction);
    // The clock moved by the difference; an edit in progress moves with it.
    self->_edit = gnss::addMinutes(self->_edit, self->_app->timeZoneMinutes() - before);
    self->refreshEdit();
    char buf[12];
    gnss::formatOffset(self->_app->timeZoneMinutes(), buf, sizeof(buf));
    lv_label_set_text(self->_offset, buf);
}

void TimeScreen::stepThunk(lv_event_t *event)
{
    auto *step = static_cast<Step *>(lv_event_get_user_data(event));
    if (!step || !step->screen) return;
    TimeScreen *self = step->screen;
    self->_edit = gnss::stepDateField(self->_edit, step->field, step->direction);
    self->refreshEdit();
}

void TimeScreen::saveThunk(lv_event_t *event)
{
    auto *self = static_cast<TimeScreen *>(lv_event_get_user_data(event));
    if (!self || !self->_app) return;
    self->_app->setLocalDateTime(self->_edit);
    self->_app->showSettings();
}

} // namespace twatch_s3plus
} // namespace layertime
