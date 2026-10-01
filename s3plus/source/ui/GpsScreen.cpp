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

#include "GpsScreen.h"

#include "../app/S3PlusApp.h"
#include "S3Theme.h"
#include "TextFormat.h"

namespace layertime {
namespace twatch_s3plus {

lv_obj_t *GpsScreen::value(int32_t x, int32_t y, int32_t width, const lv_font_t *font, lv_color_t colour)
{
    lv_obj_t *l = lv_label_create(_screen);
    lv_label_set_text(l, "--");
    // Exactly one line tall: with LONG_DOT and a content height a value too
    // wide for its box wraps onto the row below it (seen in 0.2.2, where
    // the MGRS value wrapped over ALT). A fixed height makes it end in "..."
    // instead.
    lv_obj_set_size(l, width, lv_font_get_line_height(font));
    lv_obj_set_pos(l, x, y);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_color(l, colour, 0);
    lv_obj_set_style_text_font(l, font, 0);
    return l;
}

void GpsScreen::create(S3PlusApp *app)
{
    _app = app;
    _screen = theme::screen();

    // The Ultra's GPS page back button: filled gold, dark text, "< BACK".
    lv_obj_t *back = lv_button_create(_screen);
    lv_obj_set_pos(back, 4, 4);
    lv_obj_set_size(back, 64, 26);
    lv_obj_set_style_bg_color(back, theme::gold(), 0);
    lv_obj_set_style_shadow_width(back, 0, 0);
    lv_obj_add_event_cb(back, backThunk, LV_EVENT_CLICKED, this);
    lv_obj_t *backText = lv_label_create(back);
    lv_label_set_text(backText, "< BACK");
    lv_obj_set_style_text_color(backText, theme::background(), 0);
    lv_obj_set_style_text_font(backText, &lv_font_montserrat_12, 0);
    lv_obj_center(backText);

    lv_obj_t *title = lv_label_create(_screen);
    lv_label_set_text(title, "GPS");
    lv_obj_set_width(title, 164);
    lv_obj_set_pos(title, 72, 6);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(title, theme::green(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);

    // As on the Ultra, the coordinates get the full width and the largest
    // type; the supporting figures drop to a two-up grid below.
    _status = value(6, 34, 228, &lv_font_montserrat_14, theme::green());
    _latitude = value(6, 52, 228, &lv_font_montserrat_16, theme::teal());
    _longitude = value(6, 72, 228, &lv_font_montserrat_16, theme::teal());

    lv_obj_t *mgrsTitle = value(6, 96, 228, &lv_font_montserrat_12, theme::gold());
    lv_label_set_text(mgrsTitle, "MGRS");
    // Montserrat 14: a full ten-digit grid ("15S UA 92025 15918") is too
    // wide for 228 px in Montserrat 16.
    _mgrs = value(6, 110, 228, &lv_font_montserrat_14, theme::white());

    _altitude = value(6, 136, 112, &lv_font_montserrat_12, theme::gold());
    _satellites = value(122, 136, 112, &lv_font_montserrat_12, theme::green());
    _accuracy = value(6, 154, 112, &lv_font_montserrat_12, theme::teal());
    _speed = value(122, 154, 112, &lv_font_montserrat_12, theme::gold());
    _course = value(6, 174, 228, &lv_font_montserrat_12, theme::green());

    lv_obj_t *footer = lv_label_create(_screen);
    lv_label_set_text(footer, "LAYERTIME  |  GPS");
    lv_obj_set_width(footer, 240);
    lv_obj_set_pos(footer, 0, 222);
    lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(footer, theme::gold(), 0);
    lv_obj_set_style_text_font(footer, &lv_font_montserrat_12, 0);
}

void GpsScreen::show()
{
    lv_screen_load(_screen);
    render();
}

void GpsScreen::render()
{
    if (!_app || lv_screen_active() != _screen) return;
    const NavigationState &nav = _app->navigation();
    const bool metric = _app->metricUnits();
    char buf[48];
    text::formatGpsStatus(nav, buf, sizeof(buf));
    lv_label_set_text(_status, buf);
    text::formatLatitude(nav, buf, sizeof(buf));
    lv_label_set_text(_latitude, buf);
    text::formatLongitude(nav, buf, sizeof(buf));
    lv_label_set_text(_longitude, buf);
    text::formatMgrs(nav, buf, sizeof(buf));
    lv_label_set_text(_mgrs, buf);
    text::formatAltitudeLine(nav, metric, buf, sizeof(buf));
    lv_label_set_text(_altitude, buf);
    text::formatSatellites(nav, buf, sizeof(buf));
    lv_label_set_text(_satellites, buf);
    text::formatAccuracy(nav, metric, buf, sizeof(buf));
    lv_label_set_text(_accuracy, buf);
    text::formatSpeed(nav, metric, buf, sizeof(buf));
    lv_label_set_text(_speed, buf);
    text::formatCourse(nav, buf, sizeof(buf));
    lv_label_set_text(_course, buf);
}

void GpsScreen::backThunk(lv_event_t *event)
{
    auto *self = static_cast<GpsScreen *>(lv_event_get_user_data(event));
    if (self && self->_app) self->_app->showHome();
}

} // namespace twatch_s3plus
} // namespace layertime
