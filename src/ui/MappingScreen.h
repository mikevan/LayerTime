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

#include "../model/AppSettings.h"
#include "../model/WatchState.h"

// Compass tooling, split out of GpsScreen. Answers the question someone
// actually arrives with - "what do I do with my compass?" - rather than
// reporting the magnetic declination and leaving the arithmetic to them.
//
// The page states the correction as an instruction rather than a bare
// number, and can work against GRID north (UTM/MGRS maps, the military
// G-M angle) or true north. Entirely offline: the WMM coefficients are
// embedded in firmware.
//
// A bearing converter (map->compass for navigating to something,
// compass->map for resection) was prototyped inline here and removed: it
// needs a keyboard and enough room to explain which of the two situations
// each field is for, so it belongs on its own page.
class MappingScreen {
public:
    using BackCallback = void (*)(void *userData);

    void create(BackCallback backCallback, void *userData);
    void show(const WatchState &state, const AppSettings &settings);
    void render(const WatchState &state, const AppSettings &settings);
    lv_obj_t *screen() const { return _screen; }

private:
    void buildMainPage();
    void buildLocationPage();
    void refresh();
    // Correction from a compass bearing to a map bearing, in degrees: the
    // plain declination for true north, or declination minus grid
    // convergence (the military G-M angle) for a UTM/MGRS map.
    bool mapOffsetDegrees(double &offsetOut) const;
    void styleToggle(lv_obj_t *button, lv_obj_t *label, bool active, lv_color_t color);
    void showLocationPage();
    void hideLocationPage();

    static void backThunk(lv_event_t *event);
    static void hereThunk(lv_event_t *event);
    static void elsewhereThunk(lv_event_t *event);
    static void gridThunk(lv_event_t *event);
    static void trueNorthThunk(lv_event_t *event);
    static void locationSaveThunk(lv_event_t *event);
    static void locationCancelThunk(lv_event_t *event);

    lv_obj_t *_screen = nullptr;
    lv_obj_t *_mainPage = nullptr;

    lv_obj_t *_hereButton = nullptr;
    lv_obj_t *_hereLabel = nullptr;
    lv_obj_t *_elsewhereButton = nullptr;
    lv_obj_t *_elsewhereLabel = nullptr;
    lv_obj_t *_locationText = nullptr;
    lv_obj_t *_sourceText = nullptr;

    lv_obj_t *_declValue = nullptr;
    lv_obj_t *_adviceText = nullptr;

    lv_obj_t *_gridButton = nullptr;
    lv_obj_t *_gridLabel = nullptr;
    lv_obj_t *_trueButton = nullptr;
    lv_obj_t *_trueLabel = nullptr;


    lv_obj_t *_locationPage = nullptr;
    lv_obj_t *_locationInput = nullptr;
    lv_obj_t *_locationKeyboard = nullptr;
    lv_obj_t *_locationError = nullptr;

    bool _useManualLocation = false;
    bool _useGridNorth = true;
    double _manualLatitude = 0.0;
    double _manualLongitude = 0.0;

    double _declinationDeg = 0.0;
    double _convergenceDeg = 0.0;
    bool _haveFix = false;

    const WatchState *_lastState = nullptr;
    const AppSettings *_lastSettings = nullptr;

    BackCallback _backCallback = nullptr;
    void *_userData = nullptr;
};
