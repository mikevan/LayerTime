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

namespace {
struct UtmCoordinate {
    bool valid = false;
    int zone = 0;
    char band = '-';
    double easting = 0.0;
    double northing = 0.0;
};

constexpr double kPi = 3.14159265358979323846;
constexpr double kWgs84A = 6378137.0;
constexpr double kWgs84EccSquared = 0.00669437999014;
constexpr double kUtmScaleFactor = 0.9996;

double degreesToRadians(double degrees)
{
    return degrees * kPi / 180.0;
}

char latitudeBand(double latitude)
{
    static const char bands[] = "CDEFGHJKLMNPQRSTUVWXX";
    if (latitude < -80.0 || latitude > 84.0) {
        return '-';
    }

    const int index = static_cast<int>((latitude + 80.0) / 8.0);
    return bands[index];
}

UtmCoordinate toUtm(double latitude, double longitude)
{
    UtmCoordinate result;
    if (latitude < -80.0 || latitude > 84.0 || longitude < -180.0 || longitude > 180.0) {
        return result;
    }

    const int zone = GeoGrid::utmZoneFor(latitude, longitude);
    const char band = latitudeBand(latitude);
    if (band == '-') {
        return result;
    }

    const double latRad = degreesToRadians(latitude);
    const double lonRad = degreesToRadians(longitude);
    const double lonOrigin = (zone - 1) * 6.0 - 180.0 + 3.0;
    const double lonOriginRad = degreesToRadians(lonOrigin);

    const double eccPrimeSquared =
        kWgs84EccSquared / (1.0 - kWgs84EccSquared);
    const double sinLat = sin(latRad);
    const double cosLat = cos(latRad);
    const double tanLat = tan(latRad);

    const double n = kWgs84A / sqrt(1.0 - kWgs84EccSquared * sinLat * sinLat);
    const double t = tanLat * tanLat;
    const double c = eccPrimeSquared * cosLat * cosLat;
    const double a = cosLat * (lonRad - lonOriginRad);

    const double ecc2 = kWgs84EccSquared;
    const double ecc3 = ecc2 * ecc2;
    const double ecc4 = ecc3 * ecc2;

    const double m = kWgs84A * (
        (1.0 - ecc2 / 4.0 - 3.0 * ecc3 / 64.0 - 5.0 * ecc4 / 256.0) * latRad
        - (3.0 * ecc2 / 8.0 + 3.0 * ecc3 / 32.0 + 45.0 * ecc4 / 1024.0) * sin(2.0 * latRad)
        + (15.0 * ecc3 / 256.0 + 45.0 * ecc4 / 1024.0) * sin(4.0 * latRad)
        - (35.0 * ecc4 / 3072.0) * sin(6.0 * latRad));

    const double a2 = a * a;
    const double a3 = a2 * a;
    const double a4 = a2 * a2;
    const double a5 = a4 * a;
    const double a6 = a3 * a3;

    double easting = kUtmScaleFactor * n * (
        a
        + (1.0 - t + c) * a3 / 6.0
        + (5.0 - 18.0 * t + t * t + 72.0 * c - 58.0 * eccPrimeSquared) * a5 / 120.0)
        + 500000.0;

    double northing = kUtmScaleFactor * (
        m
        + n * tanLat * (
            a2 / 2.0
            + (5.0 - t + 9.0 * c + 4.0 * c * c) * a4 / 24.0
            + (61.0 - 58.0 * t + t * t + 600.0 * c - 330.0 * eccPrimeSquared) * a6 / 720.0));

    if (latitude < 0.0) {
        northing += 10000000.0;
    }

    result.valid = true;
    result.zone = zone;
    result.band = band;
    result.easting = easting;
    result.northing = northing;
    return result;
}

bool toMgrs(const UtmCoordinate &utm, char *out, size_t outSize)
{
    if (!utm.valid || out == nullptr) return false;

    // 100km column letters repeat every three zones; row letters run A-V
    // with I and O omitted, offset by half the set on even zones so
    // neighbouring zones never show the same pair.
    static const char *const kColumns[3] = {"ABCDEFGH", "JKLMNPQR", "STUVWXYZ"};
    static const char kRowsOdd[] = "ABCDEFGHJKLMNPQRSTUV";
    static const char kRowsEven[] = "FGHJKLMNPQRSTUVABCDE";

    const int columnIndex = static_cast<int>(utm.easting / 100000.0) - 1;
    if (columnIndex < 0 || columnIndex > 7) return false;
    const char column = kColumns[(utm.zone - 1) % 3][columnIndex];

    const int rowIndex = static_cast<int>(fmod(utm.northing / 100000.0, 20.0));
    if (rowIndex < 0 || rowIndex > 19) return false;
    const char row = (utm.zone % 2 == 1) ? kRowsOdd[rowIndex] : kRowsEven[rowIndex];

    const long localEasting = static_cast<long>(fmod(utm.easting, 100000.0));
    const long localNorthing = static_cast<long>(fmod(utm.northing, 100000.0));
    snprintf(out, outSize, "%02d%c %c%c\n%05ld %05ld",
             utm.zone, utm.band, column, row, localEasting, localNorthing);
    return true;
}
}

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

        const UtmCoordinate utm = toUtm(state.latitude, state.longitude);
        char mgrs[32];
        if (toMgrs(utm, mgrs, sizeof(mgrs))) {
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
