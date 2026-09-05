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

#include "GpsScreen.h"

#include <cmath>
#include <stdio.h>
#include <stdlib.h>

#include "Theme.h"
#include "../services/GeoGrid.h"


void GpsScreen::create(BackCallback backCallback, void *userData)
{
    _backCallback = backCallback;
    _userData = userData;

    _screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(_screen, Theme::background(), 0);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_screen, 0, 0);
    lv_obj_set_style_pad_all(_screen, 0, 0);
    // Nothing on this page currently overflows the display, but scrolling is
    // enabled here too so any future field added to this screen is always
    // reachable rather than silently cut off.
    lv_obj_set_scroll_dir(_screen, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(_screen, LV_SCROLLBAR_MODE_AUTO);

    lv_obj_t *back = lv_button_create(_screen);
    lv_obj_set_pos(back, 12, 10);
    lv_obj_set_size(back, 92, 46);
    lv_obj_set_style_bg_color(back, Theme::gold(), 0);
    lv_obj_add_event_cb(back, backThunk, LV_EVENT_CLICKED, this);

    lv_obj_t *backText = lv_label_create(back);
    lv_label_set_text(backText, "< BACK");
    lv_obj_set_style_text_color(backText, Theme::background(), 0);
    lv_obj_set_style_text_font(backText, &lv_font_montserrat_20, 0);
    lv_obj_center(backText);

    lv_obj_t *title = lv_label_create(_screen);
    lv_label_set_text(title, "GPS");
    lv_obj_set_width(title, 190);
    lv_obj_set_pos(title, 205, 13);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(title, Theme::green(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);

    // Coordinates get the full panel width and Montserrat 28 - they are what
    // this page is for, and the old two-column layout could not fit them at
    // a readable size. The supporting stats drop to a two-up grid below.
    _status = makeValueLabel(12, 68, 388, &lv_font_montserrat_20, Theme::green());
    _latitude = makeValueLabel(12, 104, 388, &lv_font_montserrat_28, Theme::teal());
    _longitude = makeValueLabel(12, 146, 388, &lv_font_montserrat_28, Theme::teal());

    lv_obj_t *utmTitle = makeValueLabel(12, 196, 388, &lv_font_montserrat_20, Theme::gold());
    lv_label_set_text(utmTitle, "MGRS");
    _utm = makeValueLabel(12, 224, 388, &lv_font_montserrat_28, Theme::white());

    _altitude = makeValueLabel(12, 316, 188, &lv_font_montserrat_20, Theme::gold());
    _satellites = makeValueLabel(205, 316, 188, &lv_font_montserrat_20, Theme::green());
    _hdop = makeValueLabel(12, 354, 188, &lv_font_montserrat_20, Theme::teal());
    _speed = makeValueLabel(205, 354, 188, &lv_font_montserrat_20, Theme::gold());
    // Alone on its row, so it gets the full width and the label spelled out
    // rather than the COG abbreviation, which means nothing to most people.
    _course = makeValueLabel(12, 392, 386, &lv_font_montserrat_20, Theme::green());

    lv_obj_t *footer = lv_label_create(_screen);
    lv_label_set_text(footer, "LAYERTIME  |  GPS");
    lv_obj_set_width(footer, 390);
    lv_obj_set_pos(footer, 10, 460);
    lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(footer, Theme::gold(), 0);
    lv_obj_set_style_text_font(footer, &lv_font_montserrat_16, 0);
}

lv_obj_t *GpsScreen::makeValueLabel(
    int x,
    int y,
    int width,
    const lv_font_t *font,
    lv_color_t color,
    lv_text_align_t align)
{
    lv_obj_t *label = lv_label_create(_screen);
    lv_label_set_text(label, "--");
    lv_obj_set_width(label, width);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_align(label, align, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_font(label, font, 0);
    return label;
}

void GpsScreen::show(const WatchState &state, const AppSettings &settings)
{
    render(state, settings);
    lv_screen_load(_screen);
}

void GpsScreen::render(const WatchState &state, const AppSettings &settings)
{
    if (!state.gpsEnabled) {
        lv_label_set_text(_status, "GPS OFF");
        lv_label_set_text(_latitude, "LAT  --");
        lv_label_set_text(_longitude, "LON  --");
        lv_label_set_text(_utm, "NO FIX");
    } else if (!state.gpsFix) {
        lv_label_set_text(_status, "ACQUIRING");
        lv_label_set_text(_latitude, "LAT  --");
        lv_label_set_text(_longitude, "LON  --");
        lv_label_set_text(_utm, "WAITING\nFOR FIX");
    } else {
        lv_label_set_text_fmt(_status, "3D FIX");
        lv_label_set_text_fmt(_latitude, "LAT  %.6f", state.latitude);
        lv_label_set_text_fmt(_longitude, "LON  %.6f", state.longitude);

        const GeoGrid::UtmCoordinate utm = GeoGrid::toUtm(state.latitude, state.longitude);
        char mgrs[32];
        if (GeoGrid::toMgrs(utm, mgrs, sizeof(mgrs))) {
            lv_label_set_text(_utm, mgrs);
        } else {
            lv_label_set_text(_utm, "OUTSIDE\nGRID RANGE");
        }
    }

    if (state.gpsAltitudeValid) {
        if (settings.metricUnits) {
            const int altitudeM = static_cast<int>(state.altitudeFt / 3.280839895f);
            lv_label_set_text_fmt(_altitude, "ALT  %d M", altitudeM);
        } else {
            lv_label_set_text_fmt(_altitude, "ALT  %d FT", static_cast<int>(state.altitudeFt));
        }
    } else {
        lv_label_set_text(_altitude, settings.metricUnits ? "ALT  -- M" : "ALT  -- FT");
    }

    lv_label_set_text_fmt(_satellites, "SATS  %u", state.gpsSatellites);

    if (state.gpsHdopValid) {
        lv_label_set_text_fmt(_hdop, "HDOP  %.1f", state.gpsHdop);
    } else {
        lv_label_set_text(_hdop, "HDOP  --");
    }

    if (state.gpsSpeedValid) {
        if (settings.metricUnits) {
            const float kmh = state.gpsSpeedMph * 1.609344f;
            lv_label_set_text_fmt(_speed, "SPD  %.1f KM/H", kmh);
        } else {
            lv_label_set_text_fmt(_speed, "SPD  %.1f MPH", state.gpsSpeedMph);
        }
    } else {
        lv_label_set_text(_speed, settings.metricUnits ? "SPD  -- KM/H" : "SPD  -- MPH");
    }

    if (state.gpsCourseValid && state.gpsSpeedMph > 0.5f) {
        lv_label_set_text_fmt(
            _course,
            "DIRECTION OF TRAVEL  %03d DEG",
            static_cast<int>(state.gpsCourseDegrees + 0.5f));
    } else {
        lv_label_set_text(_course, "DIRECTION OF TRAVEL  --");
    }
}

void GpsScreen::backThunk(lv_event_t *event)
{
    auto *self = static_cast<GpsScreen *>(lv_event_get_user_data(event));
    if (self != nullptr && self->_backCallback != nullptr) {
        self->_backCallback(self->_userData);
    }
}
