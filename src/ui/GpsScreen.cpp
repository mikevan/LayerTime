#include "GpsScreen.h"

#include <cmath>
#include <stdio.h>

#include "Theme.h"

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

int utmZoneFor(double latitude, double longitude)
{
    int zone = static_cast<int>((longitude + 180.0) / 6.0) + 1;

    // UTM special-zone rules for southwest Norway.
    if (latitude >= 56.0 && latitude < 64.0 && longitude >= 3.0 && longitude < 12.0) {
        zone = 32;
    }

    // UTM special-zone rules for Svalbard.
    if (latitude >= 72.0 && latitude < 84.0) {
        if (longitude >= 0.0 && longitude < 9.0) zone = 31;
        else if (longitude < 21.0) zone = 33;
        else if (longitude < 33.0) zone = 35;
        else if (longitude < 42.0) zone = 37;
    }

    if (zone < 1) zone = 1;
    if (zone > 60) zone = 60;
    return zone;
}

UtmCoordinate toUtm(double latitude, double longitude)
{
    UtmCoordinate result;
    if (latitude < -80.0 || latitude > 84.0 || longitude < -180.0 || longitude > 180.0) {
        return result;
    }

    const int zone = utmZoneFor(latitude, longitude);
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

    lv_obj_t *back = lv_button_create(_screen);
    lv_obj_set_pos(back, 12, 10);
    lv_obj_set_size(back, 92, 46);
    lv_obj_set_style_bg_color(back, Theme::gold(), 0);
    lv_obj_add_event_cb(back, backThunk, LV_EVENT_CLICKED, this);

    lv_obj_t *backText = lv_label_create(back);
    lv_label_set_text(backText, "< BACK");
    lv_obj_set_style_text_color(backText, Theme::background(), 0);
    lv_obj_set_style_text_font(backText, &lv_font_montserrat_16, 0);
    lv_obj_center(backText);

    lv_obj_t *title = lv_label_create(_screen);
    lv_label_set_text(title, "GPS");
    lv_obj_set_width(title, 190);
    lv_obj_set_pos(title, 205, 13);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(title, Theme::green(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);

    // Right column: everything the GNSS receiver currently tells us.
    _status = makeValueLabel(205, 68, 195, &lv_font_montserrat_18, Theme::green());
    _latitude = makeValueLabel(205, 112, 195, &lv_font_montserrat_16, Theme::teal());
    _longitude = makeValueLabel(205, 154, 195, &lv_font_montserrat_16, Theme::teal());
    _altitude = makeValueLabel(205, 200, 195, &lv_font_montserrat_18, Theme::gold());
    _satellites = makeValueLabel(205, 244, 195, &lv_font_montserrat_18, Theme::green());
    _hdop = makeValueLabel(205, 288, 195, &lv_font_montserrat_18, Theme::teal());
    _speed = makeValueLabel(205, 332, 195, &lv_font_montserrat_18, Theme::gold());
    _course = makeValueLabel(205, 376, 195, &lv_font_montserrat_18, Theme::green());

    // Lower-left holding area: actual WGS84 -> UTM coordinates.
    lv_obj_t *utmTitle = makeValueLabel(12, 295, 178, &lv_font_montserrat_20, Theme::gold());
    lv_label_set_text(utmTitle, "UTM");

    _utm = makeValueLabel(12, 330, 178, &lv_font_montserrat_18, Theme::white());

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
        if (utm.valid) {
            lv_label_set_text_fmt(
                _utm,
                "ZONE %02d%c\nE %.0f\nN %.0f",
                utm.zone,
                utm.band,
                utm.easting,
                utm.northing);
        } else {
            lv_label_set_text(_utm, "OUTSIDE\nUTM RANGE");
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
            "COG  %03d DEG",
            static_cast<int>(state.gpsCourseDegrees + 0.5f));
    } else {
        lv_label_set_text(_course, "COG  --");
    }
}

void GpsScreen::backThunk(lv_event_t *event)
{
    auto *self = static_cast<GpsScreen *>(lv_event_get_user_data(event));
    if (self != nullptr && self->_backCallback != nullptr) {
        self->_backCallback(self->_userData);
    }
}
