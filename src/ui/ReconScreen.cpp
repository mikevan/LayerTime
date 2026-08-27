#include "ReconScreen.h"

#include <stdio.h>
#include <string>

#include "Theme.h"

namespace {
struct DetectorButton { const char *title; ReconDetector detector; };
constexpr DetectorButton kButtons[] = {
    {"ALL", ReconDetector::All}, {"DEAUTH", ReconDetector::Deauth},
    {"PWNAGOTCHI", ReconDetector::Pwnagotchi}, {"MULTISSID", ReconDetector::MultiSSID},
    {"FLOCK", ReconDetector::Flock}, {"PINEAPPLE", ReconDetector::Pineapple},
    {"AIRTAG", ReconDetector::AirTag}, {"FLIPPER", ReconDetector::Flipper},
    {"META", ReconDetector::Meta}
};
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
    lv_obj_set_style_text_font(backLabel, &lv_font_montserrat_16, 0);
    lv_obj_center(backLabel);

    lv_obj_t *title = lv_label_create(_screen);
    lv_label_set_text(title, "RECON");
    lv_obj_set_width(title, 200);
    lv_obj_set_pos(title, 105, 16);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(title, Theme::gold(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);

    _menu = lv_obj_create(_screen);
    lv_obj_set_size(_menu, 386, 390);
    lv_obj_set_pos(_menu, 12, 62);
    lv_obj_set_style_bg_opa(_menu, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_menu, 0, 0);
    lv_obj_set_style_pad_all(_menu, 4, 0);
    lv_obj_set_flex_flow(_menu, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_menu, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(_menu, LV_DIR_VER);

    for (size_t i = 0; i < sizeof(kButtons) / sizeof(kButtons[0]); ++i) {
        _buttonContexts[i] = {this, kButtons[i].detector};
        lv_obj_t *button = lv_button_create(_menu);
        lv_obj_set_size(button, 350, 48);
        lv_obj_set_style_bg_color(button, Theme::background(), 0);
        lv_obj_set_style_border_color(button, i == 0 ? Theme::gold() : Theme::teal(), 0);
        lv_obj_set_style_border_width(button, 2, 0);
        lv_obj_set_style_radius(button, 8, 0);
        lv_obj_add_event_cb(button, detectorThunk, LV_EVENT_CLICKED, &_buttonContexts[i]);
        lv_obj_t *label = lv_label_create(button);
        lv_label_set_text(label, kButtons[i].title);
        lv_obj_set_style_text_color(label, i == 0 ? Theme::gold() : Theme::white(), 0);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
        lv_obj_center(label);
    }

    _monitor = lv_obj_create(_screen);
    lv_obj_set_size(_monitor, 386, 390);
    lv_obj_set_pos(_monitor, 12, 62);
    lv_obj_set_style_bg_opa(_monitor, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_monitor, 0, 0);
    lv_obj_set_style_pad_all(_monitor, 0, 0);
    lv_obj_add_flag(_monitor, LV_OBJ_FLAG_HIDDEN);
    _status = lv_label_create(_monitor);
    lv_obj_set_width(_status, 370);
    lv_obj_set_pos(_status, 8, 4);
    lv_obj_set_style_text_align(_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(_status, Theme::gold(), 0);
    lv_obj_set_style_text_font(_status, &lv_font_montserrat_16, 0);
    _results = lv_label_create(_monitor);
    lv_obj_set_size(_results, 370, 340);
    lv_obj_set_pos(_results, 8, 38);
    lv_label_set_long_mode(_results, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(_results, Theme::white(), 0);
    lv_obj_set_style_text_font(_results, &lv_font_montserrat_14, 0);

    _alert = lv_obj_create(_screen);
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
    lv_obj_set_style_text_font(_alertText, &lv_font_montserrat_16, 0);
    lv_obj_t *dismiss = lv_button_create(_alert);
    lv_obj_set_size(dismiss, 180, 48);
    lv_obj_set_pos(dismiss, 91, 188);
    lv_obj_set_style_bg_color(dismiss, Theme::gold(), 0);
    lv_obj_add_event_cb(dismiss, dismissThunk, LV_EVENT_CLICKED, this);
    lv_obj_t *dismissLabel = lv_label_create(dismiss);
    lv_label_set_text(dismissLabel, "DISMISS");
    lv_obj_set_style_text_color(dismissLabel, Theme::background(), 0);
    lv_obj_center(dismissLabel);

    lv_obj_t *footer = lv_label_create(_screen);
    lv_label_set_text(footer, "LAYERTIME");
    lv_obj_set_width(footer, 390);
    lv_obj_set_pos(footer, 10, 466);
    lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(footer, Theme::gold(), 0);
    lv_obj_set_style_text_font(footer, &lv_font_montserrat_16, 0);
}

void ReconScreen::show() { renderMenu(); lv_screen_load(_screen); }
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
    lv_obj_clear_flag(_monitor, LV_OBJ_FLAG_HIDDEN);
    renderMonitor();
}

void ReconScreen::renderMenu()
{
    lv_obj_clear_flag(_menu, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_monitor, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_alert, LV_OBJ_FLAG_HIDDEN);
}

void ReconScreen::renderMonitor()
{
    const ReconStatus &s = _service->status();
    lv_label_set_text_fmt(_status, "%s | MONITORING | %u",
                          ReconService::detectorName(s.detector), static_cast<unsigned>(s.detectionCount));
    std::string text = s.detectionCount ? "" : "No activity detected.";
    for (size_t i = 0; i < s.detectionCount; ++i) {
        const ReconDetection &d = s.detections[i];
        char line[150];
        if (d.channel)
            snprintf(line, sizeof(line), "%s\n%s\n%s  %d dBm  CH %u\n\n",
                     d.category, d.detail, d.address, static_cast<int>(d.rssi), static_cast<unsigned>(d.channel));
        else
            snprintf(line, sizeof(line), "%s\n%s\n%s  %d dBm\n\n",
                     d.category, d.detail, d.address, static_cast<int>(d.rssi));
        text += line;
    }
    lv_label_set_text(_results, text.c_str());
}

void ReconScreen::renderAlert()
{
    const ReconStatus &s = _service->status();
    if (!s.detectionCount) return;
    const ReconDetection &d = s.detections[s.detectionCount - 1];
    lv_label_set_text_fmt(_alertText, "%s\n%s\n%s\n%d dBm",
                          d.category, d.detail, d.address, static_cast<int>(d.rssi));
    _renderedEventSerial = s.eventSerial;
    lv_obj_clear_flag(_alert, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(_alert);
    if (lv_screen_active() != _screen) lv_screen_load(_screen);
}

void ReconScreen::backThunk(lv_event_t *event)
{
    auto *self = static_cast<ReconScreen *>(lv_event_get_user_data(event));
    if (!self) return;
    if (self->_service && self->_service->status().monitoring) {
        self->_service->exitManualMode(); self->renderMenu(); return;
    }
    if (self->_backCallback) self->_backCallback(self->_userData);
}

void ReconScreen::detectorThunk(lv_event_t *event)
{
    auto *context = static_cast<ButtonContext *>(lv_event_get_user_data(event));
    if (context && context->screen) context->screen->selectDetector(context->detector);
}

void ReconScreen::dismissThunk(lv_event_t *event)
{
    auto *self = static_cast<ReconScreen *>(lv_event_get_user_data(event));
    if (!self || !self->_service) return;
    self->_service->acknowledgeAlert();
    lv_obj_add_flag(self->_alert, LV_OBJ_FLAG_HIDDEN);
}
