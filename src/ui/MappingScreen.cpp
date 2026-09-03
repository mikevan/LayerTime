#include "MappingScreen.h"

#include <cmath>
#include <stdio.h>

#include "Theme.h"
#include "../services/DeclinationCalculator.h"
#include "../services/GeoGrid.h"

namespace {
lv_obj_t *makeLabel(lv_obj_t *parent, const char *text, int x, int y, int width,
                    const lv_font_t *font, lv_color_t color,
                    lv_text_align_t align = LV_TEXT_ALIGN_LEFT)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, width);
    lv_obj_set_style_text_align(label, align, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_font(label, font, 0);
    return label;
}

lv_obj_t *makeToggle(lv_obj_t *parent, const char *text, int x, int y, int w, int h,
                     lv_event_cb_t cb, void *userData, lv_obj_t **labelOut)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, w, h);
    lv_obj_set_style_border_width(button, 2, 0);
    lv_obj_set_style_radius(button, 8, 0);
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, userData);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_center(label);
    if (labelOut != nullptr) *labelOut = label;
    return button;
}

}

void MappingScreen::create(BackCallback backCallback, void *userData)
{
    _backCallback = backCallback;
    _userData = userData;

    _screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(_screen, Theme::background(), 0);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_screen, 0, 0);
    lv_obj_set_style_pad_all(_screen, 0, 0);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    buildMainPage();
    buildLocationPage();
    hideLocationPage();
}

void MappingScreen::buildMainPage()
{
    _mainPage = lv_obj_create(_screen);
    lv_obj_set_size(_mainPage, 410, 502);
    lv_obj_set_pos(_mainPage, 0, 0);
    lv_obj_set_style_bg_opa(_mainPage, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_mainPage, 0, 0);
    lv_obj_set_style_pad_all(_mainPage, 0, 0);
    lv_obj_remove_flag(_mainPage, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *back = lv_button_create(_mainPage);
    lv_obj_set_pos(back, 12, 10);
    lv_obj_set_size(back, 92, 46);
    lv_obj_set_style_bg_color(back, Theme::gold(), 0);
    lv_obj_add_event_cb(back, backThunk, LV_EVENT_CLICKED, this);
    lv_obj_t *backText = lv_label_create(back);
    lv_label_set_text(backText, "< BACK");
    lv_obj_set_style_text_color(backText, Theme::background(), 0);
    lv_obj_set_style_text_font(backText, &lv_font_montserrat_20, 0);
    lv_obj_center(backText);

    // Named to match the button that opens it.
    makeLabel(_mainPage, "MAPPING", 150, 18, 240, &lv_font_montserrat_28,
              Theme::green(), LV_TEXT_ALIGN_CENTER);

    // --- where the correction is computed for ---
    makeLabel(_mainPage, "LOCATION", 12, 78, 96, &lv_font_montserrat_16, Theme::muted());
    _hereButton = makeToggle(_mainPage, "HERE", 118, 70, 88, 38, hereThunk, this, &_hereLabel);
    _elsewhereButton = makeToggle(_mainPage, "ELSEWHERE", 212, 70, 186, 38,
                                  elsewhereThunk, this, &_elsewhereLabel);

    _locationText = makeLabel(_mainPage, "-- , --", 12, 122, 386,
                              &lv_font_montserrat_20, Theme::white());
    _sourceText = makeLabel(_mainPage, "NO GPS FIX", 12, 150, 386,
                            &lv_font_montserrat_14, Theme::muted());

    // --- the instruction, which is the point of the screen ---
    lv_obj_t *card = lv_obj_create(_mainPage);
    lv_obj_set_pos(card, 12, 182);
    lv_obj_set_size(card, 386, 150);
    lv_obj_set_style_bg_color(card, Theme::background(), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, Theme::teal(), 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    makeLabel(card, "SET YOUR COMPASS TO", 0, 14, 382, &lv_font_montserrat_16,
              Theme::gold(), LV_TEXT_ALIGN_CENTER);
    _declValue = makeLabel(card, "NO LOCATION", 0, 44, 382, &lv_font_montserrat_28,
                           Theme::muted(), LV_TEXT_ALIGN_CENTER);
    _adviceText = makeLabel(card, "Set a location to get a correction.", 12, 92, 362,
                            &lv_font_montserrat_16, Theme::teal(), LV_TEXT_ALIGN_CENTER);
    lv_label_set_long_mode(_adviceText, LV_LABEL_LONG_WRAP);

    // --- which north the map uses ---
    makeLabel(_mainPage, "MAP NORTH", 12, 362, 104, &lv_font_montserrat_16, Theme::muted());
    _gridButton = makeToggle(_mainPage, "GRID", 124, 354, 88, 38, gridThunk, this, &_gridLabel);
    _trueButton = makeToggle(_mainPage, "TRUE", 218, 354, 88, 38, trueNorthThunk, this, &_trueLabel);

    // --- both conversions, on the page, always visible ---
    makeLabel(_mainPage, "Offline - NOAA WMM2025", 12, 442, 386,
              &lv_font_montserrat_14, Theme::muted(), LV_TEXT_ALIGN_CENTER);

    styleToggle(_hereButton, _hereLabel, true, Theme::green());
    styleToggle(_elsewhereButton, _elsewhereLabel, false, Theme::green());
    styleToggle(_gridButton, _gridLabel, true, Theme::blue());
    styleToggle(_trueButton, _trueLabel, false, Theme::blue());
}

void MappingScreen::buildLocationPage()
{
    _locationPage = lv_obj_create(_screen);
    lv_obj_set_size(_locationPage, 410, 502);
    lv_obj_set_pos(_locationPage, 0, 0);
    lv_obj_set_style_bg_color(_locationPage, Theme::background(), 0);
    lv_obj_set_style_bg_opa(_locationPage, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_locationPage, 0, 0);
    lv_obj_set_style_pad_all(_locationPage, 0, 0);
    lv_obj_remove_flag(_locationPage, LV_OBJ_FLAG_SCROLLABLE);

    makeLabel(_locationPage, "ENTER LOCATION", 0, 18, 410, &lv_font_montserrat_24,
              Theme::green(), LV_TEXT_ALIGN_CENTER);
    makeLabel(_locationPage, "Decimal degrees, LAT, LON:", 20, 60, 370,
              &lv_font_montserrat_16, Theme::muted());

    _locationInput = lv_textarea_create(_locationPage);
    lv_obj_set_pos(_locationInput, 20, 90);
    lv_obj_set_size(_locationInput, 370, 56);
    lv_textarea_set_one_line(_locationInput, true);
    lv_textarea_set_max_length(_locationInput, 30);
    lv_textarea_set_placeholder_text(_locationInput, "47.6062, -122.3321");
    lv_obj_set_style_text_font(_locationInput, &lv_font_montserrat_20, 0);

    _locationError = makeLabel(_locationPage, "", 20, 152, 370,
                               &lv_font_montserrat_14, Theme::danger());
    lv_label_set_long_mode(_locationError, LV_LABEL_LONG_WRAP);

    lv_obj_t *cancel = lv_button_create(_locationPage);
    lv_obj_set_pos(cancel, 20, 186);
    lv_obj_set_size(cancel, 170, 44);
    lv_obj_set_style_bg_color(cancel, Theme::background(), 0);
    lv_obj_set_style_border_color(cancel, Theme::muted(), 0);
    lv_obj_set_style_border_width(cancel, 2, 0);
    lv_obj_add_event_cb(cancel, locationCancelThunk, LV_EVENT_CLICKED, this);
    lv_obj_t *cancelLabel = lv_label_create(cancel);
    lv_label_set_text(cancelLabel, "CANCEL");
    lv_obj_set_style_text_color(cancelLabel, Theme::white(), 0);
    lv_obj_set_style_text_font(cancelLabel, &lv_font_montserrat_20, 0);
    lv_obj_center(cancelLabel);

    lv_obj_t *save = lv_button_create(_locationPage);
    lv_obj_set_pos(save, 220, 186);
    lv_obj_set_size(save, 170, 44);
    lv_obj_set_style_bg_color(save, Theme::green(), 0);
    lv_obj_add_event_cb(save, locationSaveThunk, LV_EVENT_CLICKED, this);
    lv_obj_t *saveLabel = lv_label_create(save);
    lv_label_set_text(saveLabel, "SAVE");
    lv_obj_set_style_text_color(saveLabel, Theme::background(), 0);
    lv_obj_set_style_text_font(saveLabel, &lv_font_montserrat_20, 0);
    lv_obj_center(saveLabel);

    _locationKeyboard = lv_keyboard_create(_locationPage);
    lv_obj_set_size(_locationKeyboard, 410, 246);
    lv_obj_set_pos(_locationKeyboard, 0, 256);
    lv_keyboard_set_textarea(_locationKeyboard, _locationInput);
}

void MappingScreen::styleToggle(lv_obj_t *button, lv_obj_t *label, bool active,
                                lv_color_t color)
{
    if (button == nullptr) return;
    // Filled when selected, outlined when not - so which of the pair is live
    // reads at a glance rather than needing to be inferred from the values.
    lv_obj_set_style_bg_color(button, active ? color : Theme::background(), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(button, color, 0);
    if (label != nullptr) {
        lv_obj_set_style_text_color(label, active ? Theme::background() : color, 0);
    }
}

void MappingScreen::show(const WatchState &state, const AppSettings &settings)
{
    _lastState = &state;
    _lastSettings = &settings;
    hideLocationPage();
    refresh();
    lv_screen_load(_screen);
}

void MappingScreen::render(const WatchState &state, const AppSettings &settings)
{
    _lastState = &state;
    _lastSettings = &settings;

    // 12th-order spherical harmonics - only worth running while this screen
    // is the one actually on display.
    if (_screen != nullptr && lv_screen_active() == _screen &&
        _locationPage != nullptr && lv_obj_has_flag(_locationPage, LV_OBJ_FLAG_HIDDEN)) {
        refresh();
    }
}

bool MappingScreen::mapOffsetDegrees(double &offsetOut) const
{
    if (!_haveFix) return false;
    // True north: the plain declination. Grid north: the military G-M angle,
    // declination minus grid convergence - which is what a UTM/MGRS map
    // needs, and differs from true north by up to about 3 degrees.
    offsetOut = _useGridNorth ? (_declinationDeg - _convergenceDeg) : _declinationDeg;
    return true;
}

void MappingScreen::refresh()
{
    if (_lastState == nullptr) return;

    double lat = 0.0, lon = 0.0;
    const char *source = "NO GPS FIX";
    _haveFix = false;

    if (_useManualLocation) {
        lat = _manualLatitude;
        lon = _manualLongitude;
        source = "MANUAL LOCATION";
        _haveFix = true;
    } else if (_lastState->gpsFix) {
        lat = _lastState->latitude;
        lon = _lastState->longitude;
        source = "FROM GPS FIX";
        _haveFix = true;
    }

    lv_label_set_text(_sourceText, source);

    if (!_haveFix) {
        lv_label_set_text(_locationText, "-- , --");
        lv_label_set_text(_declValue, "NO LOCATION");
        lv_obj_set_style_text_color(_declValue, Theme::muted(), 0);
        lv_label_set_text(_adviceText,
                          "Waiting for a GPS fix. Tap ELSEWHERE to enter a location instead.");
            return;
    }

    lv_label_set_text_fmt(_locationText, "%.6f, %.6f", lat, lon);

    const double year = DeclinationCalculator::decimalYear(
        _lastState->year, _lastState->month, _lastState->day);
    _declinationDeg = DeclinationCalculator::declinationDegrees(lat, lon, year);
    _convergenceDeg = GeoGrid::convergenceDegrees(lat, lon);

    double offset = 0.0;
    mapOffsetDegrees(offset);

    if (offset >= 0.0) {
        lv_label_set_text_fmt(_declValue, "%.1f DEG EAST", offset);
    } else {
        lv_label_set_text_fmt(_declValue, "%.1f DEG WEST", -offset);
    }
    lv_obj_set_style_text_color(_declValue, Theme::white(), 0);

    // Stated as an action, in the direction the user is going, so nobody has
    // to reason about the sign convention standing in a field.
    const char *northName = _useGridNorth ? "grid" : "true";
    if (offset >= 0.0) {
        lv_label_set_text_fmt(
            _adviceText,
            "Compass reads low. ADD %.1f deg to a compass bearing to get a %s bearing.",
            offset, northName);
    } else {
        lv_label_set_text_fmt(
            _adviceText,
            "Compass reads high. SUBTRACT %.1f deg from a compass bearing to get a %s bearing.",
            -offset, northName);
    }

}

void MappingScreen::showLocationPage()
{
    char buf[40];
    if (_useManualLocation) {
        snprintf(buf, sizeof(buf), "%.6f, %.6f", _manualLatitude, _manualLongitude);
    } else {
        buf[0] = '\0';
    }
    lv_textarea_set_text(_locationInput, buf);
    lv_label_set_text(_locationError, "");
    lv_obj_add_flag(_mainPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(_locationPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_state(_locationInput, LV_STATE_FOCUSED);
}

void MappingScreen::hideLocationPage()
{
    lv_obj_add_flag(_locationPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(_mainPage, LV_OBJ_FLAG_HIDDEN);
}

void MappingScreen::backThunk(lv_event_t *event)
{
    auto *self = static_cast<MappingScreen *>(lv_event_get_user_data(event));
    if (self != nullptr && self->_backCallback != nullptr) {
        self->_backCallback(self->_userData);
    }
}

void MappingScreen::hereThunk(lv_event_t *event)
{
    auto *self = static_cast<MappingScreen *>(lv_event_get_user_data(event));
    if (self == nullptr) return;
    self->_useManualLocation = false;
    self->styleToggle(self->_hereButton, self->_hereLabel, true, Theme::green());
    self->styleToggle(self->_elsewhereButton, self->_elsewhereLabel, false, Theme::green());
    self->refresh();
}

void MappingScreen::elsewhereThunk(lv_event_t *event)
{
    auto *self = static_cast<MappingScreen *>(lv_event_get_user_data(event));
    if (self != nullptr) self->showLocationPage();
}

void MappingScreen::gridThunk(lv_event_t *event)
{
    auto *self = static_cast<MappingScreen *>(lv_event_get_user_data(event));
    if (self == nullptr) return;
    self->_useGridNorth = true;
    self->styleToggle(self->_gridButton, self->_gridLabel, true, Theme::blue());
    self->styleToggle(self->_trueButton, self->_trueLabel, false, Theme::blue());
    self->refresh();
}

void MappingScreen::trueNorthThunk(lv_event_t *event)
{
    auto *self = static_cast<MappingScreen *>(lv_event_get_user_data(event));
    if (self == nullptr) return;
    self->_useGridNorth = false;
    self->styleToggle(self->_gridButton, self->_gridLabel, false, Theme::blue());
    self->styleToggle(self->_trueButton, self->_trueLabel, true, Theme::blue());
    self->refresh();
}



void MappingScreen::locationSaveThunk(lv_event_t *event)
{
    auto *self = static_cast<MappingScreen *>(lv_event_get_user_data(event));
    if (self == nullptr) return;

    const char *text = lv_textarea_get_text(self->_locationInput);
    double lat = 0.0, lon = 0.0;
    const int parsed = (text != nullptr) ? sscanf(text, "%lf , %lf", &lat, &lon) : 0;

    if (parsed != 2 || lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
        lv_label_set_text(self->_locationError,
                          "INVALID - USE LAT, LON (E.G. 47.6062, -122.3321)");
        return;
    }

    self->_useManualLocation = true;
    self->_manualLatitude = lat;
    self->_manualLongitude = lon;
    self->styleToggle(self->_hereButton, self->_hereLabel, false, Theme::green());
    self->styleToggle(self->_elsewhereButton, self->_elsewhereLabel, true, Theme::green());
    self->hideLocationPage();
    self->refresh();
}

void MappingScreen::locationCancelThunk(lv_event_t *event)
{
    auto *self = static_cast<MappingScreen *>(lv_event_get_user_data(event));
    if (self != nullptr) self->hideLocationPage();
}
