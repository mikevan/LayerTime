#pragma once

#include <lvgl.h>

#include "../model/AppSettings.h"
#include "../model/WatchState.h"

class GpsScreen {
public:
    using BackCallback = void (*)(void *userData);

    void create(BackCallback backCallback, void *userData);
    void show(const WatchState &state, const AppSettings &settings);
    void render(const WatchState &state, const AppSettings &settings);
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

    // Declination tool: a WMM-based magnetic declination lookup (auto from
    // the current GPS fix, or a manually entered location) plus a
    // magnetic<->true bearing converter for use with a physical compass.
    // Reached via a DECLINATION button on the main GPS page; entirely
    // offline, no network involved.
    void buildDeclinationPage();
    void buildManualLocationPage();
    void buildBearingPage();
    void showDeclinationPage();
    void hideDeclinationPage();
    void showManualLocationPage();
    void hideManualLocationPage();
    void showBearingPage();
    void hideBearingPage();
    void renderDeclinationPage();

    static void declinationThunk(lv_event_t *event);
    static void declinationBackThunk(lv_event_t *event);
    static void useGpsLocationThunk(lv_event_t *event);
    static void enterLocationThunk(lv_event_t *event);
    static void manualLocationSaveThunk(lv_event_t *event);
    static void manualLocationCancelThunk(lv_event_t *event);
    static void bearingConverterThunk(lv_event_t *event);
    static void bearingBackThunk(lv_event_t *event);
    static void bearingConvertThunk(lv_event_t *event);

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

    lv_obj_t *_declinationPage = nullptr;
    lv_obj_t *_declLocationText = nullptr;
    lv_obj_t *_declSourceText = nullptr;
    lv_obj_t *_declValue = nullptr;
    lv_obj_t *_declDateText = nullptr;

    lv_obj_t *_manualLocationPage = nullptr;
    lv_obj_t *_manualLocationInput = nullptr;
    lv_obj_t *_manualLocationKeyboard = nullptr;
    lv_obj_t *_manualLocationError = nullptr;

    lv_obj_t *_bearingPage = nullptr;
    lv_obj_t *_bearingInput = nullptr;
    lv_obj_t *_bearingKeyboard = nullptr;
    lv_obj_t *_bearingMagToTrue = nullptr;
    lv_obj_t *_bearingTrueToMag = nullptr;
    lv_obj_t *_bearingError = nullptr;

    bool _useManualLocation = false;
    double _manualLatitude = 0.0;
    double _manualLongitude = 0.0;
    // Last declination computed by renderDeclinationPage(), reused by the
    // bearing converter so it doesn't need its own copy of the current
    // location/state.
    double _lastDeclinationDeg = 0.0;
    bool _lastDeclinationValid = false;

    const WatchState *_lastState = nullptr;
    const AppSettings *_lastSettings = nullptr;

    BackCallback _backCallback = nullptr;
    void *_userData = nullptr;
};
