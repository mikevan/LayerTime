#include "SettingsScreen.h"

#include "Theme.h"
#include <string.h>

namespace {
enum DateTimeControl {
    DAY_DOWN,
    DAY_UP,
    MONTH_DOWN,
    MONTH_UP,
    YEAR_DOWN,
    YEAR_UP,
    HOUR_DOWN,
    HOUR_UP,
    MINUTE_DOWN,
    MINUTE_UP
};

int daysInMonth(int year, int month)
{
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2) {
        const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        return leap ? 29 : 28;
    }
    return days[month - 1];
}
}

void SettingsScreen::create(
    AppSettings &settings,
    const WatchState &state,
    SdCardService &sdCard,
    BackCallback backCallback,
    SettingsChangedCallback settingsChangedCallback,
    DateTimeSaveCallback dateTimeSaveCallback,
    void *userData)
{
    _settings = &settings;
    _state = &state;
    _sdCard = &sdCard;
    _backCallback = backCallback;
    _settingsChangedCallback = settingsChangedCallback;
    _dateTimeSaveCallback = dateTimeSaveCallback;
    _userData = userData;

    _screen = lv_obj_create(nullptr);
    lv_obj_set_user_data(_screen, this);
    lv_obj_set_style_bg_color(_screen, Theme::background(), 0);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_screen, 0, 0);
    lv_obj_set_style_pad_all(_screen, 0, 0);

    buildMainPage();
    buildDateTimePage();
    buildSdPage();
    buildMeshtasticNamePage();
    showMainPage();
}

void SettingsScreen::show()
{
    refreshMainValues();
    showMainPage();
    lv_screen_load(_screen);
}

lv_obj_t *SettingsScreen::makeLabel(
    lv_obj_t *parent,
    const char *text,
    int x,
    int y,
    int width,
    const lv_font_t *font,
    lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, width);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    return label;
}

lv_obj_t *SettingsScreen::makeButton(
    lv_obj_t *parent,
    const char *text,
    int x,
    int y,
    int width,
    int height,
    lv_event_cb_t callback,
    void *userData)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x101A1A), 0);
    lv_obj_set_style_border_color(button, Theme::teal(), 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_radius(button, 8, 0);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, userData == nullptr ? this : userData);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, Theme::white(), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_center(label);
    return button;
}

void SettingsScreen::buildMainPage()
{
    _mainPage = lv_obj_create(_screen);
    lv_obj_set_size(_mainPage, 410, 502);
    lv_obj_set_pos(_mainPage, 0, 0);
    lv_obj_set_style_bg_opa(_mainPage, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_mainPage, 0, 0);
    lv_obj_set_style_pad_all(_mainPage, 0, 0);
    // The page now holds more rows than fit on screen at once - scroll to reach them.
    lv_obj_set_scroll_dir(_mainPage, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(_mainPage, LV_SCROLLBAR_MODE_AUTO);

    lv_obj_t *title = makeLabel(
        _mainPage,
        "LAYERTIME SETTINGS",
        20,
        18,
        370,
        &lv_font_montserrat_20,
        Theme::gold());
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    makeButton(_mainPage, "DATE / TIME", 25, 62, 360, 52, dateTimeThunk);

    makeLabel(_mainPage, "BRIGHTNESS", 30, 135, 160, &lv_font_montserrat_16, Theme::white());
    _brightnessValue = makeLabel(_mainPage, "80", 310, 135, 70, &lv_font_montserrat_16, Theme::teal());
    lv_obj_set_style_text_align(_brightnessValue, LV_TEXT_ALIGN_RIGHT, 0);

    _brightnessSlider = lv_slider_create(_mainPage);
    lv_obj_set_pos(_brightnessSlider, 30, 165);
    lv_obj_set_size(_brightnessSlider, 350, 24);
    lv_slider_set_range(_brightnessSlider, 20, 255);
    lv_obj_add_event_cb(_brightnessSlider, brightnessThunk, LV_EVENT_VALUE_CHANGED, this);

    makeButton(_mainPage, "CLOCK FORMAT", 25, 214, 250, 52, clockModeThunk);
    _clockValue = makeLabel(_mainPage, "12 H", 290, 230, 90, &lv_font_montserrat_16, Theme::teal());
    lv_obj_set_style_text_align(_clockValue, LV_TEXT_ALIGN_CENTER, 0);

    makeButton(_mainPage, "UNITS", 25, 282, 250, 52, unitsThunk);
    _unitsValue = makeLabel(_mainPage, "IMPERIAL", 282, 298, 105, &lv_font_montserrat_16, Theme::teal());
    lv_obj_set_style_text_align(_unitsValue, LV_TEXT_ALIGN_CENTER, 0);

    makeButton(_mainPage, "GPS", 25, 350, 250, 52, gpsThunk);
    _gpsValue = makeLabel(_mainPage, "ON", 290, 366, 90, &lv_font_montserrat_16, Theme::teal());
    lv_obj_set_style_text_align(_gpsValue, LV_TEXT_ALIGN_CENTER, 0);

    makeButton(_mainPage, "MESHCORE", 25, 414, 250, 42, meshEnabledThunk);
    _meshEnabledValue = makeLabel(_mainPage, "OFF", 290, 426, 90, &lv_font_montserrat_16, Theme::teal());
    lv_obj_set_style_text_align(_meshEnabledValue, LV_TEXT_ALIGN_CENTER, 0);

    makeButton(_mainPage, "MESHCORE ADVERTISE", 25, 462, 250, 42, meshAdvertiseThunk);
    _meshAdvertiseValue = makeLabel(_mainPage, "OFF", 290, 474, 90, &lv_font_montserrat_16, Theme::teal());
    lv_obj_set_style_text_align(_meshAdvertiseValue, LV_TEXT_ALIGN_CENTER, 0);

    makeButton(_mainPage, "MESHTASTIC", 25, 510, 250, 42, meshtasticEnabledThunk);
    _meshtasticEnabledValue = makeLabel(_mainPage, "OFF", 290, 522, 90, &lv_font_montserrat_16, Theme::teal());
    lv_obj_set_style_text_align(_meshtasticEnabledValue, LV_TEXT_ALIGN_CENTER, 0);

    makeButton(_mainPage, "MESHTASTIC ADVERTISE", 25, 558, 250, 42, meshtasticAdvertiseThunk);
    _meshtasticAdvertiseValue = makeLabel(_mainPage, "OFF", 290, 570, 90, &lv_font_montserrat_16, Theme::teal());
    lv_obj_set_style_text_align(_meshtasticAdvertiseValue, LV_TEXT_ALIGN_CENTER, 0);

    makeButton(_mainPage, "MESHTASTIC NAME", 25, 606, 250, 42, meshtasticNameThunk);
    _meshtasticNameValue = makeLabel(_mainPage, "AUTO", 290, 618, 90, &lv_font_montserrat_16, Theme::teal());
    lv_obj_set_style_text_align(_meshtasticNameValue, LV_TEXT_ALIGN_CENTER, 0);

    makeButton(_mainPage, "EARLY WARNING", 25, 654, 250, 42, earlyWarningThunk);
    _earlyWarningValue = makeLabel(_mainPage, "ON", 290, 666, 90, &lv_font_montserrat_16, Theme::teal());
    lv_obj_set_style_text_align(_earlyWarningValue, LV_TEXT_ALIGN_CENTER, 0);

    makeButton(_mainPage, "SD LOGGING", 25, 702, 250, 42, reconSdLoggingThunk);
    _reconSdLoggingValue = makeLabel(_mainPage, "OFF", 290, 714, 90, &lv_font_montserrat_16, Theme::teal());
    lv_obj_set_style_text_align(_reconSdLoggingValue, LV_TEXT_ALIGN_CENTER, 0);

    makeButton(_mainPage, "SD CARD", 25, 750, 360, 42, sdCardThunk);

    makeButton(_mainPage, "BACK", 25, 798, 360, 32, backThunk);
}

void SettingsScreen::buildDateTimePage()
{
    _dateTimePage = lv_obj_create(_screen);
    lv_obj_set_size(_dateTimePage, 410, 502);
    lv_obj_set_pos(_dateTimePage, 0, 0);
    lv_obj_set_style_bg_opa(_dateTimePage, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_dateTimePage, 0, 0);
    lv_obj_set_style_pad_all(_dateTimePage, 0, 0);

    lv_obj_t *title = makeLabel(
        _dateTimePage,
        "DATE / TIME",
        20,
        18,
        370,
        &lv_font_montserrat_20,
        Theme::gold());
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    _editDate = makeLabel(_dateTimePage, "AUG 23 2026", 45, 66, 320, &lv_font_montserrat_28, Theme::teal());
    lv_obj_set_style_text_align(_editDate, LV_TEXT_ALIGN_CENTER, 0);

    makeButton(_dateTimePage, "DAY -", 25, 118, 110, 46, dateTimeControlThunk, reinterpret_cast<void *>(DAY_DOWN + 1));
    makeButton(_dateTimePage, "DAY +", 275, 118, 110, 46, dateTimeControlThunk, reinterpret_cast<void *>(DAY_UP + 1));
    makeButton(_dateTimePage, "MONTH -", 25, 174, 110, 46, dateTimeControlThunk, reinterpret_cast<void *>(MONTH_DOWN + 1));
    makeButton(_dateTimePage, "MONTH +", 275, 174, 110, 46, dateTimeControlThunk, reinterpret_cast<void *>(MONTH_UP + 1));
    makeButton(_dateTimePage, "YEAR -", 25, 230, 110, 46, dateTimeControlThunk, reinterpret_cast<void *>(YEAR_DOWN + 1));
    makeButton(_dateTimePage, "YEAR +", 275, 230, 110, 46, dateTimeControlThunk, reinterpret_cast<void *>(YEAR_UP + 1));

    _editTime = makeLabel(_dateTimePage, "12:00", 105, 292, 200, &lv_font_montserrat_36, Theme::gold());
    lv_obj_set_style_text_align(_editTime, LV_TEXT_ALIGN_CENTER, 0);

    makeButton(_dateTimePage, "HOUR -", 25, 345, 110, 46, dateTimeControlThunk, reinterpret_cast<void *>(HOUR_DOWN + 1));
    makeButton(_dateTimePage, "HOUR +", 275, 345, 110, 46, dateTimeControlThunk, reinterpret_cast<void *>(HOUR_UP + 1));
    makeButton(_dateTimePage, "MIN -", 145, 345, 58, 46, dateTimeControlThunk, reinterpret_cast<void *>(MINUTE_DOWN + 1));
    makeButton(_dateTimePage, "MIN +", 207, 345, 58, 46, dateTimeControlThunk, reinterpret_cast<void *>(MINUTE_UP + 1));

    makeButton(_dateTimePage, "CANCEL", 25, 425, 170, 50, cancelDateTimeThunk);
    makeButton(_dateTimePage, "SAVE", 215, 425, 170, 50, saveDateTimeThunk);
}

void SettingsScreen::buildSdPage()
{
    _sdPage = lv_obj_create(_screen);
    lv_obj_set_size(_sdPage, 410, 502);
    lv_obj_set_pos(_sdPage, 0, 0);
    lv_obj_set_style_bg_opa(_sdPage, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_sdPage, 0, 0);
    lv_obj_set_style_pad_all(_sdPage, 0, 0);

    lv_obj_t *title = makeLabel(_sdPage, "SD CARD", 20, 18, 370, &lv_font_montserrat_20, Theme::gold());
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    _sdStatusValue = makeLabel(_sdPage, "UNKNOWN", 20, 90, 370, &lv_font_montserrat_28, Theme::teal());
    lv_obj_set_style_text_align(_sdStatusValue, LV_TEXT_ALIGN_CENTER, 0);

    _sdSpaceValue = makeLabel(_sdPage, "", 20, 140, 370, &lv_font_montserrat_16, Theme::white());
    lv_obj_set_style_text_align(_sdSpaceValue, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(_sdSpaceValue, LV_LABEL_LONG_WRAP);

    _sdFormatButton = makeButton(_sdPage, "FORMAT SD CARD", 45, 210, 320, 52, sdFormatThunk);
    lv_obj_set_style_border_color(_sdFormatButton, Theme::danger(), 0);

    _sdConfirmGroup = lv_obj_create(_sdPage);
    lv_obj_set_size(_sdConfirmGroup, 410, 340);
    lv_obj_set_pos(_sdConfirmGroup, 0, 180);
    lv_obj_set_style_bg_opa(_sdConfirmGroup, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_sdConfirmGroup, 0, 0);
    lv_obj_set_style_pad_all(_sdConfirmGroup, 0, 0);
    lv_obj_add_flag(_sdConfirmGroup, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *warn = makeLabel(
        _sdConfirmGroup,
        "ERASE EVERYTHING\nON THIS CARD?",
        45,
        10,
        320,
        &lv_font_montserrat_20,
        Theme::danger());
    lv_obj_set_style_text_align(warn, LV_TEXT_ALIGN_CENTER, 0);

    makeButton(_sdConfirmGroup, "CANCEL", 25, 80, 170, 50, sdFormatCancelThunk);
    lv_obj_t *confirmBtn = makeButton(_sdConfirmGroup, "YES, ERASE", 215, 80, 170, 50, sdFormatConfirmThunk);
    lv_obj_set_style_border_color(confirmBtn, Theme::danger(), 0);

    makeButton(_sdPage, "BACK", 25, 560, 360, 32, sdBackThunk);
}

void SettingsScreen::buildMeshtasticNamePage()
{
    _meshtasticNamePage = lv_obj_create(_screen);
    lv_obj_set_size(_meshtasticNamePage, 410, 502);
    lv_obj_set_pos(_meshtasticNamePage, 0, 0);
    lv_obj_set_style_bg_color(_meshtasticNamePage, Theme::background(), 0);
    lv_obj_set_style_bg_opa(_meshtasticNamePage, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_meshtasticNamePage, 0, 0);
    lv_obj_set_style_pad_all(_meshtasticNamePage, 10, 0);
    lv_obj_add_flag(_meshtasticNamePage, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *title = makeLabel(
        _meshtasticNamePage,
        "MESHTASTIC NAME",
        60,
        8,
        290,
        &lv_font_montserrat_20,
        Theme::gold());
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    _meshtasticNameTextArea = lv_textarea_create(_meshtasticNamePage);
    lv_obj_set_size(_meshtasticNameTextArea, 380, 50);
    lv_obj_set_pos(_meshtasticNameTextArea, 5, 46);
    lv_textarea_set_one_line(_meshtasticNameTextArea, true);
    lv_textarea_set_max_length(_meshtasticNameTextArea, 19);
    lv_textarea_set_placeholder_text(_meshtasticNameTextArea, "Leave blank for auto name...");
    lv_obj_set_style_text_font(_meshtasticNameTextArea, &lv_font_montserrat_16, 0);

    makeButton(_meshtasticNamePage, "CANCEL", 25, 106, 170, 50, meshtasticNameCancelThunk);
    lv_obj_t *saveBtn = makeButton(_meshtasticNamePage, "SAVE", 215, 106, 170, 50, meshtasticNameSaveThunk);
    lv_obj_set_style_bg_color(saveBtn, Theme::green(), 0);

    _meshtasticNameKeyboard = lv_keyboard_create(_meshtasticNamePage);
    lv_obj_set_size(_meshtasticNameKeyboard, 390, 305);
    lv_obj_set_pos(_meshtasticNameKeyboard, 0, 177);
    lv_keyboard_set_textarea(_meshtasticNameKeyboard, _meshtasticNameTextArea);
}


void SettingsScreen::showMainPage()
{
    lv_obj_remove_flag(_mainPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_dateTimePage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_sdPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_meshtasticNamePage, LV_OBJ_FLAG_HIDDEN);
}

void SettingsScreen::showSdPage()
{
    if (_sdCard != nullptr) {
        _sdCard->refresh();
    }
    lv_obj_add_flag(_sdConfirmGroup, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(_sdFormatButton, LV_OBJ_FLAG_HIDDEN);
    refreshSdValues();

    lv_obj_add_flag(_mainPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_dateTimePage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_meshtasticNamePage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(_sdPage, LV_OBJ_FLAG_HIDDEN);
}

void SettingsScreen::showMeshtasticNamePage()
{
    lv_textarea_set_text(_meshtasticNameTextArea, _settings->meshtasticNodeName);
    lv_obj_add_flag(_mainPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_dateTimePage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_sdPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(_meshtasticNamePage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_state(_meshtasticNameTextArea, LV_STATE_FOCUSED);
}

void SettingsScreen::showDateTimePage()
{
    _editYear = _state->year;
    _editMonth = _state->month;
    _editDay = _state->day;
    _editHour = _state->hour;
    _editMinute = _state->minute;

    if (_editYear < 2024 || _editYear > 2099) {
        _editYear = 2026;
    }
    if (_editMonth < 1 || _editMonth > 12) {
        _editMonth = 1;
    }
    normalizeDate();

    refreshDateTimeValues();
    lv_obj_add_flag(_mainPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_sdPage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_meshtasticNamePage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(_dateTimePage, LV_OBJ_FLAG_HIDDEN);
}

void SettingsScreen::refreshMainValues()
{
    lv_slider_set_value(_brightnessSlider, _settings->brightness, LV_ANIM_OFF);
    lv_label_set_text_fmt(_brightnessValue, "%u", _settings->brightness);
    lv_label_set_text(_clockValue, _settings->use24Hour ? "24 H" : "12 H");
    lv_label_set_text(_unitsValue, _settings->metricUnits ? "METRIC" : "IMPERIAL");
    lv_label_set_text(_gpsValue, _settings->gpsEnabled ? "ON" : "OFF");
    lv_label_set_text(_meshEnabledValue, _settings->meshEnabled ? "ON" : "OFF");
    lv_label_set_text(_meshAdvertiseValue, _settings->meshAdvertiseEnabled ? "ON" : "OFF");
    lv_label_set_text(_meshtasticEnabledValue, _settings->meshtasticEnabled ? "ON" : "OFF");
    lv_label_set_text(_meshtasticAdvertiseValue, _settings->meshtasticAdvertiseEnabled ? "ON" : "OFF");
    lv_label_set_text(_meshtasticNameValue, _settings->meshtasticNodeName[0] != '\0' ? _settings->meshtasticNodeName : "AUTO");
    lv_label_set_text(_earlyWarningValue, _settings->reconEarlyWarningEnabled ? "ON" : "OFF");
    lv_label_set_text(_reconSdLoggingValue, _settings->reconSdLoggingEnabled ? "ON" : "OFF");
}

void SettingsScreen::refreshSdValues()
{
    if (_sdCard == nullptr) {
        return;
    }

    if (_sdCard->status() == SdCardStatus::Ready) {
        lv_label_set_text(_sdStatusValue, "READY");
        lv_obj_set_style_text_color(_sdStatusValue, Theme::green(), 0);
        const unsigned long long usedMb = _sdCard->usedBytes() / (1024ULL * 1024ULL);
        const unsigned long long totalMb = _sdCard->totalBytes() / (1024ULL * 1024ULL);
        lv_label_set_text_fmt(_sdSpaceValue, "%llu / %llu MB USED", usedMb, totalMb);
    } else {
        lv_label_set_text(_sdStatusValue, "NOT RECOGNIZED");
        lv_obj_set_style_text_color(_sdStatusValue, Theme::danger(), 0);
        lv_label_set_text(_sdSpaceValue, "No card, or an unreadable filesystem.\nTap FORMAT to prepare it.");
    }
}

void SettingsScreen::refreshDateTimeValues()
{
    static const char *months[] = {
        "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
        "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
    };

    lv_label_set_text_fmt(
        _editDate,
        "%s %02d %04d",
        months[_editMonth - 1],
        _editDay,
        _editYear);
    lv_label_set_text_fmt(_editTime, "%02d:%02d", _editHour, _editMinute);
}

void SettingsScreen::normalizeDate()
{
    if (_editMonth < 1) {
        _editMonth = 12;
    } else if (_editMonth > 12) {
        _editMonth = 1;
    }

    if (_editYear < 2024) {
        _editYear = 2024;
    } else if (_editYear > 2099) {
        _editYear = 2099;
    }

    const int maxDay = daysInMonth(_editYear, _editMonth);
    if (_editDay < 1) {
        _editDay = maxDay;
    } else if (_editDay > maxDay) {
        _editDay = 1;
    }
}

void SettingsScreen::backThunk(lv_event_t *event)
{
    auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(event));
    if (self->_backCallback != nullptr) {
        self->_backCallback(self->_userData);
    }
}

void SettingsScreen::dateTimeThunk(lv_event_t *event)
{
    auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(event));
    self->showDateTimePage();
}

void SettingsScreen::brightnessThunk(lv_event_t *event)
{
    auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(event));
    self->_settings->brightness = static_cast<uint8_t>(lv_slider_get_value(self->_brightnessSlider));
    lv_label_set_text_fmt(self->_brightnessValue, "%u", self->_settings->brightness);

    if (self->_settingsChangedCallback != nullptr) {
        self->_settingsChangedCallback(self->_userData);
    }
}

void SettingsScreen::clockModeThunk(lv_event_t *event)
{
    auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(event));
    self->_settings->use24Hour = !self->_settings->use24Hour;
    self->refreshMainValues();

    if (self->_settingsChangedCallback != nullptr) {
        self->_settingsChangedCallback(self->_userData);
    }
}

void SettingsScreen::unitsThunk(lv_event_t *event)
{
    auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(event));
    self->_settings->metricUnits = !self->_settings->metricUnits;
    self->refreshMainValues();

    if (self->_settingsChangedCallback != nullptr) {
        self->_settingsChangedCallback(self->_userData);
    }
}

void SettingsScreen::gpsThunk(lv_event_t *event)
{
    auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(event));
    self->_settings->gpsEnabled = !self->_settings->gpsEnabled;
    self->refreshMainValues();

    if (self->_settingsChangedCallback != nullptr) {
        self->_settingsChangedCallback(self->_userData);
    }
}

void SettingsScreen::meshEnabledThunk(lv_event_t *event)
{
    auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(event));
    self->_settings->meshEnabled = !self->_settings->meshEnabled;
    // MeshCore and Meshtastic share one physical radio - only one may be on.
    if (self->_settings->meshEnabled) {
        self->_settings->meshtasticEnabled = false;
    }
    self->refreshMainValues();

    if (self->_settingsChangedCallback != nullptr) {
        self->_settingsChangedCallback(self->_userData);
    }
}

void SettingsScreen::meshtasticEnabledThunk(lv_event_t *event)
{
    auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(event));
    self->_settings->meshtasticEnabled = !self->_settings->meshtasticEnabled;
    // MeshCore and Meshtastic share one physical radio - only one may be on.
    if (self->_settings->meshtasticEnabled) {
        self->_settings->meshEnabled = false;
    }
    self->refreshMainValues();

    if (self->_settingsChangedCallback != nullptr) {
        self->_settingsChangedCallback(self->_userData);
    }
}

void SettingsScreen::meshtasticAdvertiseThunk(lv_event_t *event)
{
    auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(event));
    self->_settings->meshtasticAdvertiseEnabled = !self->_settings->meshtasticAdvertiseEnabled;
    self->refreshMainValues();

    if (self->_settingsChangedCallback != nullptr) {
        self->_settingsChangedCallback(self->_userData);
    }
}

void SettingsScreen::meshtasticNameThunk(lv_event_t *event)
{
    auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(event));
    self->showMeshtasticNamePage();
}

void SettingsScreen::meshtasticNameSaveThunk(lv_event_t *event)
{
    auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(event));
    const char *text = lv_textarea_get_text(self->_meshtasticNameTextArea);
    strncpy(self->_settings->meshtasticNodeName, text, sizeof(self->_settings->meshtasticNodeName) - 1);
    self->_settings->meshtasticNodeName[sizeof(self->_settings->meshtasticNodeName) - 1] = '\0';
    self->showMainPage();
    self->refreshMainValues();

    if (self->_settingsChangedCallback != nullptr) {
        self->_settingsChangedCallback(self->_userData);
    }
}

void SettingsScreen::meshtasticNameCancelThunk(lv_event_t *event)
{
    auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(event));
    self->showMainPage();
}

void SettingsScreen::meshAdvertiseThunk(lv_event_t *event)
{
    auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(event));
    self->_settings->meshAdvertiseEnabled = !self->_settings->meshAdvertiseEnabled;
    self->refreshMainValues();

    if (self->_settingsChangedCallback != nullptr) {
        self->_settingsChangedCallback(self->_userData);
    }
}

void SettingsScreen::earlyWarningThunk(lv_event_t *event)
{
    auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(event));
    self->_settings->reconEarlyWarningEnabled = !self->_settings->reconEarlyWarningEnabled;
    self->refreshMainValues();

    if (self->_settingsChangedCallback != nullptr) {
        self->_settingsChangedCallback(self->_userData);
    }
}

void SettingsScreen::reconSdLoggingThunk(lv_event_t *event)
{
    auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(event));
    self->_settings->reconSdLoggingEnabled = !self->_settings->reconSdLoggingEnabled;
    self->refreshMainValues();

    if (self->_settingsChangedCallback != nullptr) {
        self->_settingsChangedCallback(self->_userData);
    }
}

void SettingsScreen::sdCardThunk(lv_event_t *event)
{
    auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(event));
    self->showSdPage();
}

void SettingsScreen::sdBackThunk(lv_event_t *event)
{
    auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(event));
    self->showMainPage();
}

void SettingsScreen::sdFormatThunk(lv_event_t *event)
{
    auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(event));
    lv_obj_add_flag(self->_sdFormatButton, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(self->_sdConfirmGroup, LV_OBJ_FLAG_HIDDEN);
}

void SettingsScreen::sdFormatCancelThunk(lv_event_t *event)
{
    auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(event));
    lv_obj_add_flag(self->_sdConfirmGroup, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(self->_sdFormatButton, LV_OBJ_FLAG_HIDDEN);
}

void SettingsScreen::sdFormatConfirmThunk(lv_event_t *event)
{
    auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(event));
    if (self->_sdCard != nullptr) {
        self->_sdCard->formatAndMount();
    }
    lv_obj_add_flag(self->_sdConfirmGroup, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(self->_sdFormatButton, LV_OBJ_FLAG_HIDDEN);
    self->refreshSdValues();
}

void SettingsScreen::dateTimeControlThunk(lv_event_t *event)
{
    auto *button = lv_event_get_target_obj(event);
    void *raw = lv_event_get_user_data(event);
    const int control = static_cast<int>(reinterpret_cast<intptr_t>(raw)) - 1;

    // The event user data on these ten controls is the control id, not `this`.
    // Recover the SettingsScreen from the screen's user data.
    lv_obj_t *page = lv_obj_get_parent(button);
    lv_obj_t *screen = lv_obj_get_parent(page);
    auto *self = static_cast<SettingsScreen *>(lv_obj_get_user_data(screen));
    if (self == nullptr) {
        return;
    }

    switch (control) {
        case DAY_DOWN: self->_editDay--; break;
        case DAY_UP: self->_editDay++; break;
        case MONTH_DOWN: self->_editMonth--; break;
        case MONTH_UP: self->_editMonth++; break;
        case YEAR_DOWN: self->_editYear--; break;
        case YEAR_UP: self->_editYear++; break;
        case HOUR_DOWN: self->_editHour = (self->_editHour + 23) % 24; break;
        case HOUR_UP: self->_editHour = (self->_editHour + 1) % 24; break;
        case MINUTE_DOWN: self->_editMinute = (self->_editMinute + 59) % 60; break;
        case MINUTE_UP: self->_editMinute = (self->_editMinute + 1) % 60; break;
    }

    self->normalizeDate();
    self->refreshDateTimeValues();
}

void SettingsScreen::saveDateTimeThunk(lv_event_t *event)
{
    auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(event));
    if (self->_dateTimeSaveCallback != nullptr) {
        self->_dateTimeSaveCallback(
            self->_editYear,
            self->_editMonth,
            self->_editDay,
            self->_editHour,
            self->_editMinute,
            self->_userData);
    }
    self->showMainPage();
}

void SettingsScreen::cancelDateTimeThunk(lv_event_t *event)
{
    auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(event));
    self->showMainPage();
}
