#include "ReconScreen.h"

#include <stdio.h>
#include <string>

#include "Theme.h"

namespace {
// Top-level group rows, in menu order beneath ALL. Index-aligned with
// ReconScreen::_groupPages. Membership itself lives in ReconService so the
// menu and the radio scheduler can never disagree about it.
constexpr ReconDetector kGroups[] = {
    ReconDetector::Trackers, ReconDetector::CounterSurveil, ReconDetector::CounterIntrusion};
constexpr size_t kGroupCount = sizeof(kGroups) / sizeof(kGroups[0]);
}

void ReconScreen::create(ReconService *service, BackCallback backCallback, void *userData)
{
    _service = service;
    _backCallback = backCallback;
    _userData = userData;
    _screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(_screen, Theme::background(), 0);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(_screen, 0, 0);

    lv_obj_t *back = lv_button_create(_screen);
    lv_obj_set_size(back, 82, 40);
    lv_obj_set_pos(back, 12, 12);
    lv_obj_set_style_bg_color(back, Theme::background(), 0);
    lv_obj_set_style_border_color(back, Theme::gold(), 0);
    lv_obj_set_style_border_width(back, 2, 0);
    lv_obj_set_style_radius(back, 8, 0);
    lv_obj_add_event_cb(back, backThunk, LV_EVENT_CLICKED, this);
    lv_obj_t *backLabel = lv_label_create(back);
    lv_label_set_text(backLabel, "BACK");
    lv_obj_set_style_text_color(backLabel, Theme::gold(), 0);
    lv_obj_set_style_text_font(backLabel, &lv_font_montserrat_20, 0);
    lv_obj_center(backLabel);

    _title = lv_label_create(_screen);
    lv_label_set_text(_title, "RECON");
    lv_obj_set_width(_title, 300);
    lv_obj_set_pos(_title, 105, 16);
    lv_obj_set_style_text_align(_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(_title, Theme::gold(), 0);
    lv_obj_set_style_text_font(_title, &lv_font_montserrat_24, 0);

    buildMenuPages();

    _monitor = lv_obj_create(_screen);
    lv_obj_set_size(_monitor, 386, 390);
    lv_obj_set_pos(_monitor, 12, 62);
    lv_obj_set_style_bg_opa(_monitor, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_monitor, 0, 0);
    lv_obj_set_style_pad_all(_monitor, 0, 0);
    lv_obj_add_flag(_monitor, LV_OBJ_FLAG_HIDDEN);
    _status = lv_label_create(_monitor);
    lv_obj_set_width(_status, 258);
    lv_obj_set_pos(_status, 4, 8);
    lv_obj_set_style_text_align(_status, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_color(_status, Theme::gold(), 0);
    lv_obj_set_style_text_font(_status, &lv_font_montserrat_20, 0);

    lv_obj_t *clearLog = lv_button_create(_monitor);
    lv_obj_set_size(clearLog, 108, 34);
    lv_obj_set_pos(clearLog, 262, 2);
    lv_obj_set_style_bg_color(clearLog, Theme::background(), 0);
    lv_obj_set_style_border_color(clearLog, Theme::danger(), 0);
    lv_obj_set_style_border_width(clearLog, 2, 0);
    lv_obj_set_style_radius(clearLog, 8, 0);
    lv_obj_add_event_cb(clearLog, clearLogThunk, LV_EVENT_CLICKED, this);
    lv_obj_t *clearLogLabel = lv_label_create(clearLog);
    lv_label_set_text(clearLogLabel, "CLEAR LOG");
    lv_obj_set_style_text_color(clearLogLabel, Theme::danger(), 0);
    lv_obj_set_style_text_font(clearLogLabel, &lv_font_montserrat_14, 0);
    lv_obj_center(clearLogLabel);

    // Wrapped in its own scrollable container so the (now persistent, up to
    // 40-entry) threat log can grow past what fits on screen without hiding
    // older entries - scroll to reach them instead of them being clipped.
    lv_obj_t *resultsBox = lv_obj_create(_monitor);
    lv_obj_set_size(resultsBox, 370, 340);
    lv_obj_set_pos(resultsBox, 8, 38);
    lv_obj_set_style_bg_opa(resultsBox, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(resultsBox, 0, 0);
    lv_obj_set_style_pad_all(resultsBox, 0, 0);
    lv_obj_set_scroll_dir(resultsBox, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(resultsBox, LV_SCROLLBAR_MODE_AUTO);

    _results = lv_label_create(resultsBox);
    lv_obj_set_width(_results, 354);
    lv_obj_set_pos(_results, 0, 0);
    lv_label_set_long_mode(_results, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(_results, Theme::white(), 0);
    lv_obj_set_style_text_font(_results, &lv_font_montserrat_18, 0);

    // Parented to LVGL's top layer, not _screen: this makes the alert a true
    // global overlay that renders above whatever screen is currently active
    // (watch face, GPS, Settings, Mesh chat, ...) instead of only appearing
    // while the Recon screen itself is loaded, and it never needs to trigger
    // a screen change to be seen or dismissed.
    _alert = lv_obj_create(lv_layer_top());
    lv_obj_set_size(_alert, 370, 260);
    lv_obj_center(_alert);
    lv_obj_set_style_bg_color(_alert, Theme::background(), 0);
    lv_obj_set_style_bg_opa(_alert, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(_alert, lv_color_hex(0xFF3030), 0);
    lv_obj_set_style_border_width(_alert, 4, 0);
    lv_obj_set_style_radius(_alert, 12, 0);
    lv_obj_add_flag(_alert, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *alertTitle = lv_label_create(_alert);
    lv_label_set_text(alertTitle, "RECON ALERT");
    lv_obj_set_width(alertTitle, 340);
    lv_obj_set_pos(alertTitle, 10, 16);
    lv_obj_set_style_text_align(alertTitle, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(alertTitle, lv_color_hex(0xFF3030), 0);
    lv_obj_set_style_text_font(alertTitle, &lv_font_montserrat_24, 0);
    _alertText = lv_label_create(_alert);
    lv_obj_set_size(_alertText, 330, 110);
    lv_obj_set_pos(_alertText, 15, 62);
    lv_label_set_long_mode(_alertText, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(_alertText, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(_alertText, Theme::white(), 0);
    lv_obj_set_style_text_font(_alertText, &lv_font_montserrat_20, 0);
    lv_obj_t *dismiss = lv_button_create(_alert);
    lv_obj_set_size(dismiss, 180, 48);
    lv_obj_set_pos(dismiss, 91, 188);
    lv_obj_set_style_bg_color(dismiss, Theme::gold(), 0);
    lv_obj_add_event_cb(dismiss, dismissThunk, LV_EVENT_CLICKED, this);
    lv_obj_t *dismissLabel = lv_label_create(dismiss);
    lv_label_set_text(dismissLabel, "DISMISS");
    lv_obj_set_style_text_color(dismissLabel, Theme::background(), 0);
    lv_obj_set_style_text_font(dismissLabel, &lv_font_montserrat_20, 0);
    lv_obj_center(dismissLabel);

    lv_obj_t *footer = lv_label_create(_screen);
    lv_label_set_text(footer, "LAYERTIME");
    lv_obj_set_width(footer, 390);
    lv_obj_set_pos(footer, 10, 466);
    lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(footer, Theme::gold(), 0);
    lv_obj_set_style_text_font(footer, &lv_font_montserrat_16, 0);
}

void ReconScreen::show(ReconDetector detector)
{
    if (detector == ReconDetector::None) {
        _openGroup = ReconDetector::None;
        renderMenu();
    } else {
        selectDetector(detector);
    }
    lv_screen_load(_screen);
}
void ReconScreen::render()
{
    if (!_service) return;
    const ReconStatus &s = _service->status();
    if (s.monitoring) renderMonitor();
    if (s.alertPending && s.eventSerial != _renderedEventSerial) renderAlert();
}

void ReconScreen::selectDetector(ReconDetector detector)
{
    if (!_service) return;
    _service->startDetector(detector);
    lv_obj_add_flag(_menu, LV_OBJ_FLAG_HIDDEN);
    for (size_t i = 0; i < kGroupCount; ++i)
        if (_groupPages[i]) lv_obj_add_flag(_groupPages[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_monitor, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(_title, ReconService::detectorName(detector));
    renderMonitor();
}

void ReconScreen::openGroup(ReconDetector group)
{
    _openGroup = group;
    showMenuLevel(group);
}

void ReconScreen::renderMenu()
{
    // Note: does NOT touch _alert. It lives on the top layer, independent of
    // Recon's own menu/monitor sub-pages, so navigating within (or away
    // from) the Recon screen never dismisses a pending alert out from under
    // the user - only the DISMISS button does that.
    showMenuLevel(_openGroup);
}

lv_obj_t *ReconScreen::createMenuPage()
{
    lv_obj_t *page = lv_obj_create(_screen);
    lv_obj_set_size(page, 386, 390);
    lv_obj_set_pos(page, 12, 62);
    lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_style_pad_all(page, 4, 0);
    lv_obj_set_flex_flow(page, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(page, LV_DIR_VER);
    return page;
}

void ReconScreen::addMenuButton(lv_obj_t *parent, size_t &index, ReconDetector detector,
                                const char *title, lv_color_t border, lv_color_t text,
                                bool opensGroup)
{
    // Hard stop rather than a silent overrun of _buttonContexts - the old
    // flat menu sized that array to exactly the button count, which would
    // have corrupted memory the first time a detector was added.
    if (index >= sizeof(_buttonContexts) / sizeof(_buttonContexts[0])) return;
    _buttonContexts[index] = {this, detector, opensGroup};
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_size(button, 350, 48);
    lv_obj_set_style_bg_color(button, Theme::background(), 0);
    lv_obj_set_style_border_color(button, border, 0);
    lv_obj_set_style_border_width(button, 2, 0);
    lv_obj_set_style_radius(button, 8, 0);
    lv_obj_add_event_cb(button, detectorThunk, LV_EVENT_CLICKED, &_buttonContexts[index]);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_color(label, text, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_center(label);
    ++index;
}

void ReconScreen::buildMenuPages()
{
    size_t index = 0;

    // Top level: ALL, then one blue row per group. Four rows at 48px in a
    // 390px page, so the level you land on most never scrolls.
    _menu = createMenuPage();
    addMenuButton(_menu, index, ReconDetector::All, "ALL", Theme::gold(), Theme::gold(), false);
    for (size_t i = 0; i < kGroupCount; ++i) {
        addMenuButton(_menu, index, kGroups[i], ReconService::detectorName(kGroups[i]),
                      Theme::blue(), Theme::blue(), true);
    }

    // One sub-page per group: ALL (the whole group sweep) plus each member.
    for (size_t i = 0; i < kGroupCount; ++i) {
        _groupPages[i] = createMenuPage();
        lv_obj_add_flag(_groupPages[i], LV_OBJ_FLAG_HIDDEN);
        addMenuButton(_groupPages[i], index, kGroups[i], "ALL",
                      Theme::gold(), Theme::gold(), false);
        size_t memberCount = 0;
        const ReconDetector *members = ReconService::groupMembers(kGroups[i], memberCount);
        for (size_t j = 0; j < memberCount; ++j) {
            addMenuButton(_groupPages[i], index, members[j],
                          ReconService::detectorName(members[j]),
                          Theme::teal(), Theme::white(), false);
        }
    }
}

void ReconScreen::showMenuLevel(ReconDetector group)
{
    if (_monitor) lv_obj_add_flag(_monitor, LV_OBJ_FLAG_HIDDEN);
    for (size_t i = 0; i < kGroupCount; ++i)
        if (_groupPages[i]) lv_obj_add_flag(_groupPages[i], LV_OBJ_FLAG_HIDDEN);

    for (size_t i = 0; i < kGroupCount; ++i) {
        if (kGroups[i] == group && _groupPages[i]) {
            lv_obj_add_flag(_menu, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(_groupPages[i], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(_title, ReconService::detectorName(group));
            return;
        }
    }

    // None, or a group with no page - show the top level rather than a
    // blank screen.
    _openGroup = ReconDetector::None;
    lv_obj_clear_flag(_menu, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(_title, "RECON");
}

void ReconScreen::renderMonitor()
{
    const ReconStatus &s = _service->status();
    lv_label_set_text_fmt(_status, "%s  x%u",
                          ReconService::detectorName(s.detector), static_cast<unsigned>(s.detectionCount));
    std::string text = s.detectionCount ? "" : "No activity detected.";
    for (size_t i = 0; i < s.detectionCount; ++i) {
        const ReconDetection &d = s.detections[i];
        char line[190];
        const char *conf = ReconService::confidenceLabel(d.confidence);
        if (d.channel)
            snprintf(line, sizeof(line), "%s  [%s]\n%s\n%s  %d dBm  CH %u  x%lu\n\n",
                     d.category, conf, d.detail, d.address, static_cast<int>(d.rssi),
                     static_cast<unsigned>(d.channel),
                     static_cast<unsigned long>(d.encounterCount));
        else
            snprintf(line, sizeof(line), "%s  [%s]\n%s\n%s  %d dBm  x%lu\n\n",
                     d.category, conf, d.detail, d.address, static_cast<int>(d.rssi),
                     static_cast<unsigned long>(d.encounterCount));
        text += line;
    }
    lv_label_set_text(_results, text.c_str());
}

void ReconScreen::renderAlert()
{
    const ReconStatus &s = _service->status();
    if (!s.detectionCount) return;
    const ReconDetection &d = s.detections[s.detectionCount - 1];
    lv_label_set_text_fmt(_alertText, "%s  [%s]\n%s\n%s\n%d dBm",
                          d.category, ReconService::confidenceLabel(d.confidence),
                          d.detail, d.address, static_cast<int>(d.rssi));
    _renderedEventSerial = s.eventSerial;
    lv_obj_clear_flag(_alert, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(_alert);
    // No lv_screen_load() here on purpose - the alert is a top-layer overlay,
    // so it's already visible above whichever screen the user is on. We
    // never change screens to show it, and dismissing it (dismissThunk) never
    // changes screens either, so the user stays exactly where they were.
}

void ReconScreen::backThunk(lv_event_t *event)
{
    auto *self = static_cast<ReconScreen *>(lv_event_get_user_data(event));
    if (!self) return;
    // Three levels, one button: monitor -> the group it was started from
    // -> the top menu -> out of Recon.
    if (self->_service && self->_service->status().monitoring) {
        self->_service->exitManualMode();
        self->showMenuLevel(self->_openGroup);
        return;
    }
    if (self->_openGroup != ReconDetector::None) {
        self->_openGroup = ReconDetector::None;
        self->showMenuLevel(ReconDetector::None);
        return;
    }
    if (self->_backCallback) self->_backCallback(self->_userData);
}

void ReconScreen::detectorThunk(lv_event_t *event)
{
    auto *context = static_cast<ButtonContext *>(lv_event_get_user_data(event));
    if (!context || !context->screen) return;
    if (context->opensGroup) context->screen->openGroup(context->detector);
    else context->screen->selectDetector(context->detector);
}

void ReconScreen::dismissThunk(lv_event_t *event)
{
    auto *self = static_cast<ReconScreen *>(lv_event_get_user_data(event));
    if (!self || !self->_service) return;
    self->_service->acknowledgeAlert();
    lv_obj_add_flag(self->_alert, LV_OBJ_FLAG_HIDDEN);
}

void ReconScreen::clearLogThunk(lv_event_t *event)
{
    auto *self = static_cast<ReconScreen *>(lv_event_get_user_data(event));
    if (!self || !self->_service) return;
    self->_service->clearDetections();
    self->renderMonitor();
}
