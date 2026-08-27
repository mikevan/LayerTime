#include "GpsScreen.h"

#include <cmath>
#include <stdio.h>
#include <stdlib.h>

#include "Theme.h"
#include "../services/DeclinationCalculator.h"

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

    // Right column: everything the GNSS receiver currently tells us.
    _status = makeValueLabel(205, 68, 195, &lv_font_montserrat_20, Theme::green());
    _latitude = makeValueLabel(205, 112, 195, &lv_font_montserrat_20, Theme::teal());
    _longitude = makeValueLabel(205, 154, 195, &lv_font_montserrat_20, Theme::teal());
    _altitude = makeValueLabel(205, 200, 195, &lv_font_montserrat_20, Theme::gold());
    _satellites = makeValueLabel(205, 244, 195, &lv_font_montserrat_20, Theme::green());
    _hdop = makeValueLabel(205, 288, 195, &lv_font_montserrat_20, Theme::teal());
    _speed = makeValueLabel(205, 332, 195, &lv_font_montserrat_20, Theme::gold());
    _course = makeValueLabel(205, 376, 195, &lv_font_montserrat_20, Theme::green());

    // Lower-left holding area: actual WGS84 -> UTM coordinates.
    lv_obj_t *utmTitle = makeValueLabel(12, 295, 178, &lv_font_montserrat_20, Theme::gold());
    lv_label_set_text(utmTitle, "UTM");

    _utm = makeValueLabel(12, 330, 178, &lv_font_montserrat_20, Theme::white());

    lv_obj_t *declinationButton = lv_button_create(_screen);
    lv_obj_set_size(declinationButton, 178, 40);
    lv_obj_set_pos(declinationButton, 12, 412);
    lv_obj_set_style_bg_color(declinationButton, Theme::background(), 0);
    lv_obj_set_style_border_color(declinationButton, Theme::teal(), 0);
    lv_obj_set_style_border_width(declinationButton, 2, 0);
    lv_obj_set_style_radius(declinationButton, 8, 0);
    lv_obj_add_event_cb(declinationButton, declinationThunk, LV_EVENT_CLICKED, this);
    lv_obj_t *declinationButtonLabel = lv_label_create(declinationButton);
    lv_label_set_text(declinationButtonLabel, "DECLINATION");
    lv_obj_set_style_text_color(declinationButtonLabel, Theme::teal(), 0);
    lv_obj_set_style_text_font(declinationButtonLabel, &lv_font_montserrat_16, 0);
    lv_obj_center(declinationButtonLabel);

    lv_obj_t *footer = lv_label_create(_screen);
    lv_label_set_text(footer, "LAYERTIME  |  GPS");
    lv_obj_set_width(footer, 390);
    lv_obj_set_pos(footer, 10, 460);
    lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(footer, Theme::gold(), 0);
    lv_obj_set_style_text_font(footer, &lv_font_montserrat_16, 0);

    buildDeclinationPage();
    buildManualLocationPage();
    buildBearingPage();
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
    // Cached so button callbacks on the declination/bearing pages (which
    // don't receive state/settings directly) can read current GPS data.
    _lastState = &state;
    _lastSettings = &settings;

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

    // The WMM computation is a bit of trig-heavy work (12th-order spherical
    // harmonics) - only run it when the declination page is actually the
    // one on screen, not on every 250ms tick regardless of what's visible.
    if (_declinationPage != nullptr && !lv_obj_has_flag(_declinationPage, LV_OBJ_FLAG_HIDDEN)) {
        renderDeclinationPage();
    }
}

void GpsScreen::backThunk(lv_event_t *event)
{
    auto *self = static_cast<GpsScreen *>(lv_event_get_user_data(event));
    if (self != nullptr && self->_backCallback != nullptr) {
        self->_backCallback(self->_userData);
    }
}

void GpsScreen::buildDeclinationPage()
{
    _declinationPage = lv_obj_create(_screen);
    lv_obj_set_size(_declinationPage, 410, 502);
    lv_obj_set_pos(_declinationPage, 0, 0);
    lv_obj_set_style_bg_color(_declinationPage, Theme::background(), 0);
    lv_obj_set_style_bg_opa(_declinationPage, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_declinationPage, 0, 0);
    lv_obj_set_style_pad_all(_declinationPage, 0, 0);
    lv_obj_set_scroll_dir(_declinationPage, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(_declinationPage, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_add_flag(_declinationPage, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *back = lv_button_create(_declinationPage);
    lv_obj_set_pos(back, 12, 10);
    lv_obj_set_size(back, 92, 46);
    lv_obj_set_style_bg_color(back, Theme::gold(), 0);
    lv_obj_add_event_cb(back, declinationBackThunk, LV_EVENT_CLICKED, this);
    lv_obj_t *backText = lv_label_create(back);
    lv_label_set_text(backText, "< BACK");
    lv_obj_set_style_text_color(backText, Theme::background(), 0);
    lv_obj_set_style_text_font(backText, &lv_font_montserrat_20, 0);
    lv_obj_center(backText);

    lv_obj_t *title = lv_label_create(_declinationPage);
    lv_label_set_text(title, "DECLINATION");
    lv_obj_set_width(title, 200);
    lv_obj_set_pos(title, 195, 20);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(title, Theme::green(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);

    lv_obj_t *locationLabel = lv_label_create(_declinationPage);
    lv_label_set_text(locationLabel, "LOCATION");
    lv_obj_set_pos(locationLabel, 14, 74);
    lv_obj_set_style_text_color(locationLabel, Theme::teal(), 0);
    lv_obj_set_style_text_font(locationLabel, &lv_font_montserrat_18, 0);

    _declLocationText = lv_label_create(_declinationPage);
    lv_obj_set_width(_declLocationText, 386);
    lv_obj_set_pos(_declLocationText, 14, 100);
    lv_obj_set_style_text_color(_declLocationText, Theme::white(), 0);
    lv_obj_set_style_text_font(_declLocationText, &lv_font_montserrat_18, 0);
    lv_label_set_text(_declLocationText, "-- , --");

    _declSourceText = lv_label_create(_declinationPage);
    lv_obj_set_width(_declSourceText, 386);
    lv_obj_set_pos(_declSourceText, 14, 128);
    lv_obj_set_style_text_color(_declSourceText, Theme::muted(), 0);
    lv_obj_set_style_text_font(_declSourceText, &lv_font_montserrat_16, 0);
    lv_label_set_text(_declSourceText, "NO GPS FIX");

    lv_obj_t *useGpsButton = lv_button_create(_declinationPage);
    lv_obj_set_size(useGpsButton, 185, 42);
    lv_obj_set_pos(useGpsButton, 14, 160);
    lv_obj_set_style_bg_color(useGpsButton, Theme::background(), 0);
    lv_obj_set_style_border_color(useGpsButton, Theme::green(), 0);
    lv_obj_set_style_border_width(useGpsButton, 2, 0);
    lv_obj_set_style_radius(useGpsButton, 8, 0);
    lv_obj_add_event_cb(useGpsButton, useGpsLocationThunk, LV_EVENT_CLICKED, this);
    lv_obj_t *useGpsLabel = lv_label_create(useGpsButton);
    lv_label_set_text(useGpsLabel, "USE GPS FIX");
    lv_obj_set_style_text_color(useGpsLabel, Theme::green(), 0);
    lv_obj_set_style_text_font(useGpsLabel, &lv_font_montserrat_16, 0);
    lv_obj_center(useGpsLabel);

    lv_obj_t *enterLocationButton = lv_button_create(_declinationPage);
    lv_obj_set_size(enterLocationButton, 185, 42);
    lv_obj_set_pos(enterLocationButton, 211, 160);
    lv_obj_set_style_bg_color(enterLocationButton, Theme::background(), 0);
    lv_obj_set_style_border_color(enterLocationButton, Theme::teal(), 0);
    lv_obj_set_style_border_width(enterLocationButton, 2, 0);
    lv_obj_set_style_radius(enterLocationButton, 8, 0);
    lv_obj_add_event_cb(enterLocationButton, enterLocationThunk, LV_EVENT_CLICKED, this);
    lv_obj_t *enterLocationLabel = lv_label_create(enterLocationButton);
    lv_label_set_text(enterLocationLabel, "ENTER LOCATION");
    lv_obj_set_style_text_color(enterLocationLabel, Theme::teal(), 0);
    lv_obj_set_style_text_font(enterLocationLabel, &lv_font_montserrat_16, 0);
    lv_obj_center(enterLocationLabel);

    _declValue = lv_label_create(_declinationPage);
    lv_obj_set_width(_declValue, 386);
    lv_obj_set_pos(_declValue, 14, 225);
    lv_obj_set_style_text_align(_declValue, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(_declValue, Theme::gold(), 0);
    lv_obj_set_style_text_font(_declValue, &lv_font_montserrat_32, 0);
    lv_label_set_text(_declValue, "NO LOCATION");

    _declDateText = lv_label_create(_declinationPage);
    lv_obj_set_width(_declDateText, 386);
    lv_obj_set_pos(_declDateText, 14, 268);
    lv_obj_set_style_text_align(_declDateText, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(_declDateText, Theme::muted(), 0);
    lv_obj_set_style_text_font(_declDateText, &lv_font_montserrat_16, 0);
    lv_label_set_text(_declDateText, "TAP ENTER LOCATION BELOW");

    lv_obj_t *bearingLabel = lv_label_create(_declinationPage);
    lv_label_set_text(bearingLabel, "BEARING CONVERTER");
    lv_obj_set_pos(bearingLabel, 14, 320);
    lv_obj_set_style_text_color(bearingLabel, Theme::teal(), 0);
    lv_obj_set_style_text_font(bearingLabel, &lv_font_montserrat_18, 0);

    lv_obj_t *bearingButton = lv_button_create(_declinationPage);
    lv_obj_set_size(bearingButton, 386, 48);
    lv_obj_set_pos(bearingButton, 14, 350);
    lv_obj_set_style_bg_color(bearingButton, Theme::background(), 0);
    lv_obj_set_style_border_color(bearingButton, Theme::gold(), 0);
    lv_obj_set_style_border_width(bearingButton, 2, 0);
    lv_obj_set_style_radius(bearingButton, 8, 0);
    lv_obj_add_event_cb(bearingButton, bearingConverterThunk, LV_EVENT_CLICKED, this);
    lv_obj_t *bearingButtonLabel = lv_label_create(bearingButton);
    lv_label_set_text(bearingButtonLabel, "CONVERT A COMPASS BEARING");
    lv_obj_set_style_text_color(bearingButtonLabel, Theme::gold(), 0);
    lv_obj_set_style_text_font(bearingButtonLabel, &lv_font_montserrat_16, 0);
    lv_obj_center(bearingButtonLabel);

    lv_obj_t *note = lv_label_create(_declinationPage);
    lv_label_set_text(note, "Offline, computed on-device (NOAA WMM2025).");
    lv_obj_set_width(note, 386);
    lv_obj_set_pos(note, 14, 410);
    lv_obj_set_style_text_align(note, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(note, Theme::muted(), 0);
    lv_obj_set_style_text_font(note, &lv_font_montserrat_14, 0);

    lv_obj_t *footer = lv_label_create(_declinationPage);
    lv_label_set_text(footer, "LAYERTIME  |  DECLINATION");
    lv_obj_set_width(footer, 390);
    lv_obj_set_pos(footer, 10, 470);
    lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(footer, Theme::gold(), 0);
    lv_obj_set_style_text_font(footer, &lv_font_montserrat_16, 0);
}

void GpsScreen::buildManualLocationPage()
{
    _manualLocationPage = lv_obj_create(_screen);
    lv_obj_set_size(_manualLocationPage, 410, 502);
    lv_obj_set_pos(_manualLocationPage, 0, 0);
    lv_obj_set_style_bg_color(_manualLocationPage, Theme::background(), 0);
    lv_obj_set_style_bg_opa(_manualLocationPage, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_manualLocationPage, 0, 0);
    lv_obj_set_style_pad_all(_manualLocationPage, 10, 0);
    lv_obj_add_flag(_manualLocationPage, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *title = lv_label_create(_manualLocationPage);
    lv_label_set_text(title, "ENTER LOCATION");
    lv_obj_set_width(title, 290);
    lv_obj_set_pos(title, 60, 8);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(title, Theme::gold(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);

    lv_obj_t *hint = lv_label_create(_manualLocationPage);
    lv_label_set_text(hint, "Decimal degrees, LAT, LON:");
    lv_obj_set_width(hint, 380);
    lv_obj_set_pos(hint, 5, 40);
    lv_obj_set_style_text_color(hint, Theme::muted(), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);

    _manualLocationInput = lv_textarea_create(_manualLocationPage);
    lv_obj_set_size(_manualLocationInput, 380, 50);
    lv_obj_set_pos(_manualLocationInput, 5, 62);
    lv_textarea_set_one_line(_manualLocationInput, true);
    lv_textarea_set_max_length(_manualLocationInput, 32);
    lv_textarea_set_placeholder_text(_manualLocationInput, "47.6062, -122.3321");
    lv_obj_set_style_text_font(_manualLocationInput, &lv_font_montserrat_18, 0);

    _manualLocationError = lv_label_create(_manualLocationPage);
    lv_label_set_text(_manualLocationError, "");
    lv_obj_set_width(_manualLocationError, 380);
    lv_obj_set_pos(_manualLocationError, 5, 118);
    lv_label_set_long_mode(_manualLocationError, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(_manualLocationError, Theme::danger(), 0);
    lv_obj_set_style_text_font(_manualLocationError, &lv_font_montserrat_14, 0);

    lv_obj_t *cancel = lv_button_create(_manualLocationPage);
    lv_obj_set_size(cancel, 170, 50);
    lv_obj_set_pos(cancel, 5, 148);
    lv_obj_set_style_bg_color(cancel, Theme::background(), 0);
    lv_obj_set_style_border_color(cancel, Theme::gold(), 0);
    lv_obj_set_style_border_width(cancel, 2, 0);
    lv_obj_add_event_cb(cancel, manualLocationCancelThunk, LV_EVENT_CLICKED, this);
    lv_obj_t *cancelLabel = lv_label_create(cancel);
    lv_label_set_text(cancelLabel, "CANCEL");
    lv_obj_set_style_text_color(cancelLabel, Theme::gold(), 0);
    lv_obj_set_style_text_font(cancelLabel, &lv_font_montserrat_18, 0);
    lv_obj_center(cancelLabel);

    lv_obj_t *save = lv_button_create(_manualLocationPage);
    lv_obj_set_size(save, 170, 50);
    lv_obj_set_pos(save, 215, 148);
    lv_obj_set_style_bg_color(save, Theme::green(), 0);
    lv_obj_add_event_cb(save, manualLocationSaveThunk, LV_EVENT_CLICKED, this);
    lv_obj_t *saveLabel = lv_label_create(save);
    lv_label_set_text(saveLabel, "SAVE");
    lv_obj_set_style_text_color(saveLabel, Theme::background(), 0);
    lv_obj_set_style_text_font(saveLabel, &lv_font_montserrat_18, 0);
    lv_obj_center(saveLabel);

    _manualLocationKeyboard = lv_keyboard_create(_manualLocationPage);
    lv_obj_set_size(_manualLocationKeyboard, 390, 275);
    lv_obj_set_pos(_manualLocationKeyboard, 0, 210);
    lv_keyboard_set_textarea(_manualLocationKeyboard, _manualLocationInput);
}

void GpsScreen::buildBearingPage()
{
    _bearingPage = lv_obj_create(_screen);
    lv_obj_set_size(_bearingPage, 410, 502);
    lv_obj_set_pos(_bearingPage, 0, 0);
    lv_obj_set_style_bg_color(_bearingPage, Theme::background(), 0);
    lv_obj_set_style_bg_opa(_bearingPage, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_bearingPage, 0, 0);
    lv_obj_set_style_pad_all(_bearingPage, 10, 0);
    lv_obj_add_flag(_bearingPage, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *back = lv_button_create(_bearingPage);
    lv_obj_set_pos(back, 2, 2);
    lv_obj_set_size(back, 86, 40);
    lv_obj_set_style_bg_color(back, Theme::background(), 0);
    lv_obj_set_style_border_color(back, Theme::gold(), 0);
    lv_obj_set_style_border_width(back, 2, 0);
    lv_obj_set_style_radius(back, 8, 0);
    lv_obj_add_event_cb(back, bearingBackThunk, LV_EVENT_CLICKED, this);
    lv_obj_t *backLabel = lv_label_create(back);
    lv_label_set_text(backLabel, "BACK");
    lv_obj_set_style_text_color(backLabel, Theme::gold(), 0);
    lv_obj_set_style_text_font(backLabel, &lv_font_montserrat_16, 0);
    lv_obj_center(backLabel);

    lv_obj_t *title = lv_label_create(_bearingPage);
    lv_label_set_text(title, "BEARING CONVERTER");
    lv_obj_set_width(title, 300);
    lv_obj_set_pos(title, 90, 8);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(title, Theme::gold(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);

    lv_obj_t *hint = lv_label_create(_bearingPage);
    lv_label_set_text(hint, "Enter a bearing, 0-359 degrees:");
    lv_obj_set_width(hint, 380);
    lv_obj_set_pos(hint, 5, 48);
    lv_obj_set_style_text_color(hint, Theme::muted(), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);

    _bearingInput = lv_textarea_create(_bearingPage);
    lv_obj_set_size(_bearingInput, 380, 50);
    lv_obj_set_pos(_bearingInput, 5, 70);
    lv_textarea_set_one_line(_bearingInput, true);
    lv_textarea_set_max_length(_bearingInput, 8);
    lv_textarea_set_placeholder_text(_bearingInput, "e.g. 275");
    lv_obj_set_style_text_font(_bearingInput, &lv_font_montserrat_18, 0);

    lv_obj_t *convert = lv_button_create(_bearingPage);
    lv_obj_set_size(convert, 380, 44);
    lv_obj_set_pos(convert, 5, 128);
    lv_obj_set_style_bg_color(convert, Theme::green(), 0);
    lv_obj_add_event_cb(convert, bearingConvertThunk, LV_EVENT_CLICKED, this);
    lv_obj_t *convertLabel = lv_label_create(convert);
    lv_label_set_text(convertLabel, "CONVERT");
    lv_obj_set_style_text_color(convertLabel, Theme::background(), 0);
    lv_obj_set_style_text_font(convertLabel, &lv_font_montserrat_18, 0);
    lv_obj_center(convertLabel);

    _bearingError = lv_label_create(_bearingPage);
    lv_label_set_text(_bearingError, "");
    lv_obj_set_width(_bearingError, 380);
    lv_obj_set_pos(_bearingError, 5, 180);
    lv_label_set_long_mode(_bearingError, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(_bearingError, Theme::danger(), 0);
    lv_obj_set_style_text_font(_bearingError, &lv_font_montserrat_14, 0);

    _bearingMagToTrue = lv_label_create(_bearingPage);
    lv_label_set_text(_bearingMagToTrue, "IF MAGNETIC (COMPASS): TRUE = --");
    lv_obj_set_width(_bearingMagToTrue, 380);
    lv_obj_set_pos(_bearingMagToTrue, 5, 208);
    lv_label_set_long_mode(_bearingMagToTrue, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(_bearingMagToTrue, Theme::teal(), 0);
    lv_obj_set_style_text_font(_bearingMagToTrue, &lv_font_montserrat_18, 0);

    _bearingTrueToMag = lv_label_create(_bearingPage);
    lv_label_set_text(_bearingTrueToMag, "IF TRUE (MAP): MAGNETIC = --");
    lv_obj_set_width(_bearingTrueToMag, 380);
    lv_obj_set_pos(_bearingTrueToMag, 5, 252);
    lv_label_set_long_mode(_bearingTrueToMag, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(_bearingTrueToMag, Theme::gold(), 0);
    lv_obj_set_style_text_font(_bearingTrueToMag, &lv_font_montserrat_18, 0);

    _bearingKeyboard = lv_keyboard_create(_bearingPage);
    lv_obj_set_size(_bearingKeyboard, 390, 210);
    lv_obj_set_pos(_bearingKeyboard, 0, 280);
    lv_keyboard_set_textarea(_bearingKeyboard, _bearingInput);
}

void GpsScreen::renderDeclinationPage()
{
    if (_lastState == nullptr || _lastSettings == nullptr) return;

    double lat = 0.0, lon = 0.0;
    bool haveLocation = false;
    const char *sourceLabel = "NO GPS FIX";

    if (_useManualLocation) {
        lat = _manualLatitude;
        lon = _manualLongitude;
        haveLocation = true;
        sourceLabel = "MANUAL LOCATION";
    } else if (_lastState->gpsFix) {
        lat = _lastState->latitude;
        lon = _lastState->longitude;
        haveLocation = true;
        sourceLabel = "GPS FIX";
    }

    lv_label_set_text(_declSourceText, sourceLabel);

    if (haveLocation) {
        lv_label_set_text_fmt(_declLocationText, "%.6f, %.6f", lat, lon);

        const double year = DeclinationCalculator::decimalYear(
            _lastState->year, _lastState->month, _lastState->day);
        const double declination = DeclinationCalculator::declinationDegrees(lat, lon, year);
        _lastDeclinationDeg = declination;
        _lastDeclinationValid = true;

        if (declination >= 0.0) {
            lv_label_set_text_fmt(_declValue, "%.1f DEG EAST", declination);
        } else {
            lv_label_set_text_fmt(_declValue, "%.1f DEG WEST", -declination);
        }
        lv_obj_set_style_text_color(_declValue, Theme::gold(), 0);

        lv_label_set_text_fmt(
            _declDateText,
            "AS OF %04d-%02d-%02d  |  WMM2025",
            _lastState->year, _lastState->month, _lastState->day);
    } else {
        lv_label_set_text(_declLocationText, "-- , --");
        lv_label_set_text(_declValue, "NO LOCATION");
        lv_obj_set_style_text_color(_declValue, Theme::muted(), 0);
        lv_label_set_text(_declDateText, "TAP ENTER LOCATION BELOW");
        _lastDeclinationValid = false;
    }
}

void GpsScreen::showDeclinationPage()
{
    renderDeclinationPage();
    lv_obj_clear_flag(_declinationPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(_declinationPage);
}

void GpsScreen::hideDeclinationPage()
{
    lv_obj_add_flag(_declinationPage, LV_OBJ_FLAG_HIDDEN);
}

void GpsScreen::showManualLocationPage()
{
    char buf[40];
    if (_useManualLocation) {
        snprintf(buf, sizeof(buf), "%.6f, %.6f", _manualLatitude, _manualLongitude);
    } else {
        buf[0] = '\0';
    }
    lv_textarea_set_text(_manualLocationInput, buf);
    lv_label_set_text(_manualLocationError, "");
    lv_obj_clear_flag(_manualLocationPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(_manualLocationPage);
    lv_obj_add_state(_manualLocationInput, LV_STATE_FOCUSED);
}

void GpsScreen::hideManualLocationPage()
{
    lv_obj_add_flag(_manualLocationPage, LV_OBJ_FLAG_HIDDEN);
}

void GpsScreen::showBearingPage()
{
    lv_textarea_set_text(_bearingInput, "");
    lv_label_set_text(_bearingError, "");
    lv_label_set_text(_bearingMagToTrue, "IF MAGNETIC (COMPASS): TRUE = --");
    lv_label_set_text(_bearingTrueToMag, "IF TRUE (MAP): MAGNETIC = --");
    lv_obj_clear_flag(_bearingPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(_bearingPage);
    lv_obj_add_state(_bearingInput, LV_STATE_FOCUSED);
}

void GpsScreen::hideBearingPage()
{
    lv_obj_add_flag(_bearingPage, LV_OBJ_FLAG_HIDDEN);
}

void GpsScreen::declinationThunk(lv_event_t *event)
{
    auto *self = static_cast<GpsScreen *>(lv_event_get_user_data(event));
    if (self != nullptr) self->showDeclinationPage();
}

void GpsScreen::declinationBackThunk(lv_event_t *event)
{
    auto *self = static_cast<GpsScreen *>(lv_event_get_user_data(event));
    if (self != nullptr) self->hideDeclinationPage();
}

void GpsScreen::useGpsLocationThunk(lv_event_t *event)
{
    auto *self = static_cast<GpsScreen *>(lv_event_get_user_data(event));
    if (self == nullptr) return;
    self->_useManualLocation = false;
    self->renderDeclinationPage();
}

void GpsScreen::enterLocationThunk(lv_event_t *event)
{
    auto *self = static_cast<GpsScreen *>(lv_event_get_user_data(event));
    if (self != nullptr) self->showManualLocationPage();
}

void GpsScreen::manualLocationSaveThunk(lv_event_t *event)
{
    auto *self = static_cast<GpsScreen *>(lv_event_get_user_data(event));
    if (self == nullptr) return;

    const char *text = lv_textarea_get_text(self->_manualLocationInput);
    double lat = 0.0, lon = 0.0;
    const int parsed = (text != nullptr) ? sscanf(text, "%lf , %lf", &lat, &lon) : 0;

    if (parsed != 2 || lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
        lv_label_set_text(self->_manualLocationError, "INVALID - USE LAT, LON (E.G. 47.6062, -122.3321)");
        return;
    }

    self->_useManualLocation = true;
    self->_manualLatitude = lat;
    self->_manualLongitude = lon;
    self->hideManualLocationPage();
    self->renderDeclinationPage();
}

void GpsScreen::manualLocationCancelThunk(lv_event_t *event)
{
    auto *self = static_cast<GpsScreen *>(lv_event_get_user_data(event));
    if (self != nullptr) self->hideManualLocationPage();
}

void GpsScreen::bearingConverterThunk(lv_event_t *event)
{
    auto *self = static_cast<GpsScreen *>(lv_event_get_user_data(event));
    if (self != nullptr) self->showBearingPage();
}

void GpsScreen::bearingBackThunk(lv_event_t *event)
{
    auto *self = static_cast<GpsScreen *>(lv_event_get_user_data(event));
    if (self != nullptr) self->hideBearingPage();
}

void GpsScreen::bearingConvertThunk(lv_event_t *event)
{
    auto *self = static_cast<GpsScreen *>(lv_event_get_user_data(event));
    if (self == nullptr) return;

    if (!self->_lastDeclinationValid) {
        lv_label_set_text(self->_bearingError, "SET A LOCATION ON THE DECLINATION PAGE FIRST");
        lv_label_set_text(self->_bearingMagToTrue, "IF MAGNETIC (COMPASS): TRUE = --");
        lv_label_set_text(self->_bearingTrueToMag, "IF TRUE (MAP): MAGNETIC = --");
        return;
    }

    const char *text = lv_textarea_get_text(self->_bearingInput);
    double bearing = 0.0;
    const int parsed = (text != nullptr) ? sscanf(text, "%lf", &bearing) : 0;
    if (parsed != 1) {
        lv_label_set_text(self->_bearingError, "ENTER A NUMBER, 0-359");
        lv_label_set_text(self->_bearingMagToTrue, "IF MAGNETIC (COMPASS): TRUE = --");
        lv_label_set_text(self->_bearingTrueToMag, "IF TRUE (MAP): MAGNETIC = --");
        return;
    }

    bearing = fmod(bearing, 360.0);
    if (bearing < 0.0) bearing += 360.0;

    const double magToTrue = fmod(bearing + self->_lastDeclinationDeg + 360.0, 360.0);
    const double trueToMag = fmod(bearing - self->_lastDeclinationDeg + 360.0, 360.0);

    lv_label_set_text(self->_bearingError, "");
    lv_label_set_text_fmt(self->_bearingMagToTrue, "IF MAGNETIC (COMPASS): TRUE = %.1f DEG", magToTrue);
    lv_label_set_text_fmt(self->_bearingTrueToMag, "IF TRUE (MAP): MAGNETIC = %.1f DEG", trueToMag);
}
