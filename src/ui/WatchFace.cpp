#include "WatchFace.h"

#include <stdio.h>
#include "Theme.h"

namespace {
const char *weekdayShort(int weekday)
{
    static const char *names[] = {
        "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
    };

    if (weekday < 0 || weekday > 6) {
        return "---";
    }
    return names[weekday];
}

const char *monthShort(int month)
{
    static const char *names[] = {
        "---", "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
        "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
    };

    if (month < 1 || month > 12) {
        return "---";
    }
    return names[month];
}
}

lv_obj_t *WatchFace::createDataLabel(
    lv_obj_t *parent,
    int x,
    int y,
    int width,
    const char *initialText)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, initialText);
    lv_obj_set_width(label, width);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label, Theme::white(), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
    return label;
}

void WatchFace::create()
{
    _screen = lv_screen_active();

    lv_obj_set_style_bg_color(_screen, Theme::background(), 0);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(_screen, 0, 0);
    lv_obj_add_flag(_screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_screen, screenEventThunk, LV_EVENT_LONG_PRESSED, this);
    // Short taps on the background only - LVGL delivers CLICKED to the
    // screen object itself just for presses that miss every child, so the
    // Mesh/Meshtastic/Recon buttons and the GPS/THREATS labels are
    // unaffected. LONG_PRESSED above still opens Settings; the two gestures
    // don't overlap.
    lv_obj_add_event_cb(_screen, backgroundTapThunk, LV_EVENT_CLICKED, this);

    // Physical Ultra display: 410 x 502 portrait.
    _owl.create(_screen, 45, 45, 320, 320);

    _battery = lv_label_create(_screen);
    lv_label_set_text(_battery, "BAT --%");
    lv_obj_set_width(_battery, 150);
    lv_obj_set_pos(_battery, 130, 12);
    lv_obj_set_style_text_align(_battery, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(_battery, Theme::gold(), 0);
    lv_obj_set_style_text_font(_battery, &lv_font_montserrat_20, 0);

    _leftTop = createDataLabel(_screen, 0, 82, 110, "ALT\n-- FT");
    _rightTop = createDataLabel(_screen, 300, 82, 110, "COG\n---");
    _leftBottom = createDataLabel(_screen, 0, 205, 110, "THREATS\n--");
    _rightBottom = createDataLabel(_screen, 300, 205, 110, "GPS\nWAIT");

    lv_obj_set_style_text_color(_leftTop, Theme::gold(), 0);
    lv_obj_set_style_text_color(_rightTop, Theme::teal(), 0);
    lv_obj_set_style_text_color(_leftBottom, Theme::green(), 0);
    lv_obj_set_style_text_color(_rightBottom, Theme::green(), 0);

    // GPS is an active touch target. Tap it to open the detailed GNSS page.
    lv_obj_add_flag(_rightBottom, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_rightBottom, gpsEventThunk, LV_EVENT_CLICKED, this);

    // THREATS is also a touch target - tap it to jump straight into Recon's
    // All-detectors monitor, skipping the detector-picker menu.
    lv_obj_add_flag(_leftBottom, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_leftBottom, threatsEventThunk, LV_EVENT_CLICKED, this);

    // Mesh shortcut: lower-left over the owl field, deliberately compact.
    _meshButton = lv_button_create(_screen);
    lv_obj_set_size(_meshButton, 88, 38);
    lv_obj_set_pos(_meshButton, 18, 300);
    lv_obj_set_style_bg_color(_meshButton, Theme::background(), 0);
    lv_obj_set_style_bg_opa(_meshButton, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(_meshButton, Theme::gold(), 0);
    lv_obj_set_style_border_width(_meshButton, 2, 0);
    lv_obj_set_style_radius(_meshButton, 8, 0);
    lv_obj_add_event_cb(_meshButton, meshEventThunk, LV_EVENT_CLICKED, this);

    lv_obj_t *meshLabel = lv_label_create(_meshButton);
    lv_label_set_text(meshLabel, "MESHCORE");
    lv_obj_set_style_text_color(meshLabel, Theme::gold(), 0);
    lv_obj_set_style_text_font(meshLabel, &lv_font_montserrat_14, 0);
    lv_obj_center(meshLabel);

    // Recon shortcut: lower-right, matching the Mesh button.
    _meshtasticButton = lv_button_create(_screen);
    lv_obj_set_size(_meshtasticButton, 88, 38);
    lv_obj_set_pos(_meshtasticButton, 161, 300);
    lv_obj_set_style_bg_color(_meshtasticButton, Theme::background(), 0);
    lv_obj_set_style_bg_opa(_meshtasticButton, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(_meshtasticButton, Theme::green(), 0);
    lv_obj_set_style_border_width(_meshtasticButton, 2, 0);
    lv_obj_set_style_radius(_meshtasticButton, 8, 0);
    lv_obj_add_event_cb(_meshtasticButton, meshtasticEventThunk, LV_EVENT_CLICKED, this);
    lv_obj_t *meshtasticLabel = lv_label_create(_meshtasticButton);
    lv_label_set_text(meshtasticLabel, "MTASTIC");
    lv_obj_set_style_text_color(meshtasticLabel, Theme::green(), 0);
    lv_obj_set_style_text_font(meshtasticLabel, &lv_font_montserrat_14, 0);
    lv_obj_center(meshtasticLabel);

    _reconButton = lv_button_create(_screen);
    lv_obj_set_size(_reconButton, 88, 38);
    lv_obj_set_pos(_reconButton, 304, 300);
    lv_obj_set_style_bg_color(_reconButton, Theme::background(), 0);
    lv_obj_set_style_bg_opa(_reconButton, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(_reconButton, Theme::teal(), 0);
    lv_obj_set_style_border_width(_reconButton, 2, 0);
    lv_obj_set_style_radius(_reconButton, 8, 0);
    lv_obj_add_event_cb(_reconButton, reconEventThunk, LV_EVENT_CLICKED, this);

    lv_obj_t *reconLabel = lv_label_create(_reconButton);
    lv_label_set_text(reconLabel, "RECON");
    lv_obj_set_style_text_color(reconLabel, Theme::teal(), 0);
    lv_obj_set_style_text_font(reconLabel, &lv_font_montserrat_16, 0);
    lv_obj_center(reconLabel);

    _time = lv_label_create(_screen);
    lv_label_set_text(_time, "00:00");
    lv_obj_set_width(_time, 300);
    lv_obj_set_pos(_time, 55, 360);
    lv_obj_set_style_text_align(_time, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(_time, Theme::gold(), 0);
    lv_obj_set_style_text_font(_time, &lv_font_montserrat_48, 0);

    _date = lv_label_create(_screen);
    lv_label_set_text(_date, "--- | --- --");
    lv_obj_set_width(_date, 260);
    lv_obj_set_pos(_date, 75, 422);
    lv_obj_set_style_text_align(_date, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(_date, Theme::teal(), 0);
    lv_obj_set_style_text_font(_date, &lv_font_montserrat_20, 0);

    lv_obj_t *footer = lv_label_create(_screen);
    lv_label_set_text(footer, "LAYERTIME  |  T-WATCH ULTRA");
    lv_obj_set_width(footer, 390);
    lv_obj_set_pos(footer, 10, 455);
    lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(footer, Theme::gold(), 0);
    lv_obj_set_style_text_font(footer, &lv_font_montserrat_20, 0);
}

void WatchFace::render(const WatchState &state, const AppSettings &settings, const ReconStatus &reconStatus)
{
    lv_label_set_text_fmt(
        _battery,
        state.batteryConnected ? "BAT %u%%" : "BAT --",
        state.batteryPercent);

    int displayHour = state.hour;

    if (!settings.use24Hour) {
        displayHour = state.hour % 12;
        if (displayHour == 0) {
            displayHour = 12;
        }
    }

    lv_label_set_text_fmt(_time, "%02d:%02d", displayHour, state.minute);

    lv_label_set_text_fmt(
        _date,
        "%s | %s %02d",
        weekdayShort(state.weekday),
        monthShort(state.month),
        state.day);

    if (state.gpsAltitudeValid) {
        if (settings.metricUnits) {
            const int altitudeM = static_cast<int>(state.altitudeFt / 3.280839895f);
            lv_label_set_text_fmt(_leftTop, "ALT\n%d M", altitudeM);
        } else {
            lv_label_set_text_fmt(_leftTop, "ALT\n%d FT", static_cast<int>(state.altitudeFt));
        }
    } else {
        lv_label_set_text(_leftTop, settings.metricUnits ? "ALT\n-- M" : "ALT\n-- FT");
    }

    // No magnetometer on this board - there is no true (stationary) heading
    // source. Show GPS course-over-ground instead, same data/threshold as
    // the GPS detail page's COG readout: direction of travel while moving,
    // blank while stationary (COG is undefined at zero speed).
    if (state.gpsCourseValid && state.gpsSpeedMph > 0.5f) {
        lv_label_set_text_fmt(_rightTop, "COG\n%03d DEG", static_cast<int>(state.gpsCourseDegrees + 0.5f));
    } else {
        lv_label_set_text(_rightTop, "COG\n--");
    }

    if (!state.gpsEnabled) {
        lv_label_set_text(_rightBottom, "GPS\nOFF");
    } else if (state.gpsFix) {
        lv_label_set_text_fmt(_rightBottom, "GPS\n3D %u", state.gpsSatellites);
    } else {
        lv_label_set_text(_rightBottom, "GPS\nWAIT");
    }

    // Status text reflects what's actively scanning right now: the selected
    // manual detector ("ALL", "DEAUTH", ...), "EARLY WARNING" when only the
    // background sweep is running, or "OFF" when nothing is scanning at all.
    const char *reconStatusText;
    if (reconStatus.monitoring) {
        reconStatusText = ReconService::detectorName(reconStatus.detector);
    } else if (reconStatus.earlyWarningEnabled) {
        reconStatusText = ReconService::detectorName(ReconDetector::EarlyWarning);
    } else {
        reconStatusText = "OFF";
    }
    lv_label_set_text_fmt(_leftBottom, "THREATS\n%s", reconStatusText);

    // Color is independent of live scan state: red as long as anything is in
    // the threat log, regardless of whether monitoring is currently running.
    // The log persists across start/stop - only the user's CLEAR LOG button
    // on the Recon screen resets it - so a threat found earlier stays flagged
    // here until the user acknowledges and clears it.
    lv_obj_set_style_text_color(_leftBottom, reconStatus.detectionCount > 0 ? Theme::danger() : Theme::green(), 0);
}

void WatchFace::setSettingsRequestedCallback(
    SettingsRequestedCallback callback,
    void *userData)
{
    _settingsRequestedCallback = callback;
    _settingsRequestedUserData = userData;
}

void WatchFace::setGpsRequestedCallback(
    GpsRequestedCallback callback,
    void *userData)
{
    _gpsRequestedCallback = callback;
    _gpsRequestedUserData = userData;
}

void WatchFace::setMeshRequestedCallback(
    MeshRequestedCallback callback,
    void *userData)
{
    _meshRequestedCallback = callback;
    _meshRequestedUserData = userData;
}

void WatchFace::setMeshtasticRequestedCallback(
    MeshtasticRequestedCallback callback,
    void *userData)
{
    _meshtasticRequestedCallback = callback;
    _meshtasticRequestedUserData = userData;
}

void WatchFace::setReconRequestedCallback(
    ReconRequestedCallback callback,
    void *userData)
{
    _reconRequestedCallback = callback;
    _reconRequestedUserData = userData;
}

void WatchFace::setThreatsRequestedCallback(
    ThreatsRequestedCallback callback,
    void *userData)
{
    _threatsRequestedCallback = callback;
    _threatsRequestedUserData = userData;
}

void WatchFace::screenEventThunk(lv_event_t *event)
{
    auto *self = static_cast<WatchFace *>(lv_event_get_user_data(event));
    if (self == nullptr || self->_settingsRequestedCallback == nullptr) {
        return;
    }

    self->_settingsRequestedCallback(self->_settingsRequestedUserData);
}

void WatchFace::gpsEventThunk(lv_event_t *event)
{
    auto *self = static_cast<WatchFace *>(lv_event_get_user_data(event));
    if (self == nullptr || self->_gpsRequestedCallback == nullptr) {
        return;
    }

    self->_gpsRequestedCallback(self->_gpsRequestedUserData);
}

void WatchFace::meshEventThunk(lv_event_t *event)
{
    auto *self = static_cast<WatchFace *>(lv_event_get_user_data(event));
    if (self == nullptr || self->_meshRequestedCallback == nullptr) {
        return;
    }

    self->_meshRequestedCallback(self->_meshRequestedUserData);
}

void WatchFace::meshtasticEventThunk(lv_event_t *event)
{
    auto *self = static_cast<WatchFace *>(lv_event_get_user_data(event));
    if (self == nullptr || self->_meshtasticRequestedCallback == nullptr) {
        return;
    }

    self->_meshtasticRequestedCallback(self->_meshtasticRequestedUserData);
}

void WatchFace::reconEventThunk(lv_event_t *event)
{
    auto *self = static_cast<WatchFace *>(lv_event_get_user_data(event));
    if (self == nullptr || self->_reconRequestedCallback == nullptr) {
        return;
    }

    self->_reconRequestedCallback(self->_reconRequestedUserData);
}

void WatchFace::threatsEventThunk(lv_event_t *event)
{
    auto *self = static_cast<WatchFace *>(lv_event_get_user_data(event));
    if (self == nullptr || self->_threatsRequestedCallback == nullptr) {
        return;
    }

    self->_threatsRequestedCallback(self->_threatsRequestedUserData);
}

void WatchFace::setBackgroundTapCallback(BackgroundTapCallback callback, void *userData)
{
    _backgroundTapCallback = callback;
    _backgroundTapUserData = userData;
}

void WatchFace::backgroundTapThunk(lv_event_t *event)
{
    auto *self = static_cast<WatchFace *>(lv_event_get_user_data(event));
    if (self == nullptr || self->_backgroundTapCallback == nullptr) {
        return;
    }

    self->_backgroundTapCallback(self->_backgroundTapUserData);
}
