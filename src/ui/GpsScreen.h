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

    BackCallback _backCallback = nullptr;
    void *_userData = nullptr;
};
