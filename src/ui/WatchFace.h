#pragma once

#include <lvgl.h>

#include "../model/AppSettings.h"
#include "../model/WatchState.h"
#include "../services/ReconService.h"
#include "OwlLogo.h"

class WatchFace {
public:
    using SettingsRequestedCallback = void (*)(void *userData);
    using GpsRequestedCallback = void (*)(void *userData);
    using MeshRequestedCallback = void (*)(void *userData);
    using MeshtasticRequestedCallback = void (*)(void *userData);
    using ReconRequestedCallback = void (*)(void *userData);
    using ThreatsRequestedCallback = void (*)(void *userData);
    // Fires on a short tap that lands on the watch-face background itself,
    // not on any of its buttons or data labels (those consume their own
    // clicks). WatchApp uses it for double-tap-to-sleep; keeping the timing
    // there rather than here means the face stays a pure view.
    using BackgroundTapCallback = void (*)(void *userData);

    void create();
    void render(const WatchState &state, const AppSettings &settings, const ReconStatus &reconStatus);
    void setSettingsRequestedCallback(SettingsRequestedCallback callback, void *userData);
    void setGpsRequestedCallback(GpsRequestedCallback callback, void *userData);
    void setMeshRequestedCallback(MeshRequestedCallback callback, void *userData);
    void setMeshtasticRequestedCallback(MeshtasticRequestedCallback callback, void *userData);
    void setReconRequestedCallback(ReconRequestedCallback callback, void *userData);
    void setThreatsRequestedCallback(ThreatsRequestedCallback callback, void *userData);
    void setBackgroundTapCallback(BackgroundTapCallback callback, void *userData);
    lv_obj_t *screen() const { return _screen; }

private:
    static void screenEventThunk(lv_event_t *event);
    static void gpsEventThunk(lv_event_t *event);
    static void meshEventThunk(lv_event_t *event);
    static void meshtasticEventThunk(lv_event_t *event);
    static void reconEventThunk(lv_event_t *event);
    static void threatsEventThunk(lv_event_t *event);
    static void backgroundTapThunk(lv_event_t *event);

    OwlLogo _owl;
    lv_obj_t *_screen = nullptr;

    lv_obj_t *_battery = nullptr;
    lv_obj_t *_time = nullptr;
    lv_obj_t *_date = nullptr;

    lv_obj_t *_leftTop = nullptr;
    lv_obj_t *_rightTop = nullptr;
    lv_obj_t *_leftBottom = nullptr;
    lv_obj_t *_rightBottom = nullptr;
    lv_obj_t *_meshButton = nullptr;
    lv_obj_t *_meshtasticButton = nullptr;
    lv_obj_t *_reconButton = nullptr;

    SettingsRequestedCallback _settingsRequestedCallback = nullptr;
    void *_settingsRequestedUserData = nullptr;
    GpsRequestedCallback _gpsRequestedCallback = nullptr;
    void *_gpsRequestedUserData = nullptr;
    MeshRequestedCallback _meshRequestedCallback = nullptr;
    void *_meshRequestedUserData = nullptr;
    MeshtasticRequestedCallback _meshtasticRequestedCallback = nullptr;
    void *_meshtasticRequestedUserData = nullptr;
    ReconRequestedCallback _reconRequestedCallback = nullptr;
    void *_reconRequestedUserData = nullptr;
    ThreatsRequestedCallback _threatsRequestedCallback = nullptr;
    void *_threatsRequestedUserData = nullptr;
    BackgroundTapCallback _backgroundTapCallback = nullptr;
    void *_backgroundTapUserData = nullptr;

    lv_obj_t *createDataLabel(
        lv_obj_t *parent,
        int x,
        int y,
        int width,
        const char *initialText);
};
