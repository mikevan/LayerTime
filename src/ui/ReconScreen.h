#pragma once

#include <lvgl.h>
#include "../services/ReconService.h"

class ReconScreen {
public:
    using BackCallback = void (*)(void *userData);
    void create(ReconService *service, BackCallback backCallback, void *userData);
    void show();
    void render();

private:
    struct ButtonContext {
        ReconScreen *screen = nullptr;
        ReconDetector detector = ReconDetector::None;
    };

    static void backThunk(lv_event_t *event);
    static void detectorThunk(lv_event_t *event);
    static void dismissThunk(lv_event_t *event);
    void selectDetector(ReconDetector detector);
    void renderMenu();
    void renderMonitor();
    void renderAlert();

    ReconService *_service = nullptr;
    BackCallback _backCallback = nullptr;
    void *_userData = nullptr;
    lv_obj_t *_screen = nullptr;
    lv_obj_t *_menu = nullptr;
    lv_obj_t *_monitor = nullptr;
    lv_obj_t *_status = nullptr;
    lv_obj_t *_results = nullptr;
    lv_obj_t *_alert = nullptr;
    lv_obj_t *_alertText = nullptr;
    ButtonContext _buttonContexts[9];
    uint32_t _renderedEventSerial = 0;
};
