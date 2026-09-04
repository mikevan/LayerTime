#pragma once

#include <lvgl.h>

#include "../model/AppSettings.h"
#include "../model/WatchState.h"
#include "../services/SdCardService.h"

class SettingsScreen {
public:
    using BackCallback = void (*)(void *userData);
    using SettingsChangedCallback = void (*)(void *userData);
    using DateTimeSaveCallback = void (*)(
        int year,
        int month,
        int day,
        int hour,
        int minute,
        void *userData);

    void create(
        AppSettings &settings,
        const WatchState &state,
        SdCardService &sdCard,
        BackCallback backCallback,
        SettingsChangedCallback settingsChangedCallback,
        DateTimeSaveCallback dateTimeSaveCallback,
        void *userData);

    void show();
    lv_obj_t *screen() const { return _screen; }

private:
    static void backThunk(lv_event_t *event);
    // Fixed BACK in the upper left, outside the scrolling pages - matches
    // Recon's, and means leaving Settings never requires scrolling to find it.
    static void topBackThunk(lv_event_t *event);
    static void dateTimeThunk(lv_event_t *event);
    static void brightnessThunk(lv_event_t *event);
    static void clockModeThunk(lv_event_t *event);
    static void unitsThunk(lv_event_t *event);
    static void gpsThunk(lv_event_t *event);
    static void meshEnabledThunk(lv_event_t *event);
    static void meshAdvertiseThunk(lv_event_t *event);
    static void meshtasticEnabledThunk(lv_event_t *event);
    static void meshtasticAdvertiseThunk(lv_event_t *event);
    static void meshtasticNameThunk(lv_event_t *event);
    static void meshtasticNameSaveThunk(lv_event_t *event);
    static void meshtasticNameCancelThunk(lv_event_t *event);
    static void earlyWarningThunk(lv_event_t *event);
    static void reconSdLoggingThunk(lv_event_t *event);
    static void sleepModeThunk(lv_event_t *event);
    static void squatchifyThunk(lv_event_t *event);
    static void dateTimeControlThunk(lv_event_t *event);
    static void saveDateTimeThunk(lv_event_t *event);
    static void cancelDateTimeThunk(lv_event_t *event);
    static void sdCardThunk(lv_event_t *event);
    static void sdBackThunk(lv_event_t *event);
    static void sdFormatThunk(lv_event_t *event);
    static void sdFormatConfirmThunk(lv_event_t *event);
    static void sdFormatCancelThunk(lv_event_t *event);

    void buildMainPage();
    void buildDateTimePage();
    void buildSdPage();
    void buildMeshtasticNamePage();
    void showMainPage();
    void showDateTimePage();
    void showSdPage();
    void showMeshtasticNamePage();
    void refreshMainValues();
    void refreshDateTimeValues();
    void refreshSdValues();
    void normalizeDate();

    lv_obj_t *makeButton(
        lv_obj_t *parent,
        const char *text,
        int x,
        int y,
        int width,
        int height,
        lv_event_cb_t callback,
        void *userData = nullptr);

    lv_obj_t *makeLabel(
        lv_obj_t *parent,
        const char *text,
        int x,
        int y,
        int width,
        const lv_font_t *font,
        lv_color_t color);

    AppSettings *_settings = nullptr;
    const WatchState *_state = nullptr;
    SdCardService *_sdCard = nullptr;
    BackCallback _backCallback = nullptr;
    SettingsChangedCallback _settingsChangedCallback = nullptr;
    DateTimeSaveCallback _dateTimeSaveCallback = nullptr;
    void *_userData = nullptr;

    lv_obj_t *_screen = nullptr;
    lv_obj_t *_mainPage = nullptr;
    lv_obj_t *_dateTimePage = nullptr;
    lv_obj_t *_sdPage = nullptr;
    lv_obj_t *_meshtasticNamePage = nullptr;
    lv_obj_t *_meshtasticNameTextArea = nullptr;
    lv_obj_t *_meshtasticNameKeyboard = nullptr;

    lv_obj_t *_brightnessSlider = nullptr;
    lv_obj_t *_brightnessValue = nullptr;
    lv_obj_t *_clockValue = nullptr;
    lv_obj_t *_unitsValue = nullptr;
    lv_obj_t *_gpsValue = nullptr;
    lv_obj_t *_meshEnabledValue = nullptr;
    lv_obj_t *_meshAdvertiseValue = nullptr;
    lv_obj_t *_meshtasticEnabledValue = nullptr;
    lv_obj_t *_meshtasticAdvertiseValue = nullptr;
    lv_obj_t *_meshtasticNameValue = nullptr;
    lv_obj_t *_earlyWarningValue = nullptr;
    lv_obj_t *_reconSdLoggingValue = nullptr;
    lv_obj_t *_sleepModeValue = nullptr;
    lv_obj_t *_squatchifyValue = nullptr;

    lv_obj_t *_sdStatusValue = nullptr;
    lv_obj_t *_sdSpaceValue = nullptr;
    lv_obj_t *_sdFormatButton = nullptr;
    lv_obj_t *_sdConfirmGroup = nullptr;

    lv_obj_t *_editDate = nullptr;
    lv_obj_t *_editTime = nullptr;

    int _editYear = 2026;
    int _editMonth = 1;
    int _editDay = 1;
    int _editHour = 0;
    int _editMinute = 0;
};
