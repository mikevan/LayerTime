// LayerTime - counter-intrusion and resilient-communications firmware
// for the LilyGo T-Watch S3 Plus.
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

#include "ReconScreen.h"

#include <stdio.h>
#include <string.h>

#include "core/logic/ReconSelection.h"

#include "../app/S3PlusApp.h"
#include "S3Theme.h"
#include "TextFormat.h"

namespace layertime {
namespace twatch_s3plus {

namespace {
constexpr ReconTarget kGroups[] = {ReconTarget::Trackers, ReconTarget::CounterSurveil,
                                   ReconTarget::CounterIntrusion};
constexpr size_t kGroupCount = sizeof(kGroups) / sizeof(kGroups[0]);

constexpr int32_t kTop = 34;            // below the header
constexpr int32_t kPageHeight = 240 - kTop;

// The results text: up to 40 events at about 110 characters each.
char gResults[ReconState::kMaxEvents * 120 + 64];
}

void ReconScreen::create(S3PlusApp *app)
{
    _app = app;
    _screen = theme::screen();

    lv_obj_t *back = theme::button(_screen, "BACK", theme::gold(), &lv_font_montserrat_12, 56, 26);
    lv_obj_set_pos(back, 4, 4);
    lv_obj_add_event_cb(back, backThunk, LV_EVENT_CLICKED, this);

    _title = lv_label_create(_screen);
    lv_obj_set_width(_title, 172);
    lv_obj_set_pos(_title, 64, 9);
    lv_label_set_long_mode(_title, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(_title, theme::gold(), 0);
    lv_obj_set_style_text_font(_title, &lv_font_montserrat_14, 0);
    lv_label_set_text(_title, "RECON");

    buildPages();

    _monitor = lv_obj_create(_screen);
    lv_obj_set_size(_monitor, 240, kPageHeight);
    lv_obj_set_pos(_monitor, 0, kTop);
    lv_obj_set_style_bg_opa(_monitor, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_monitor, 0, 0);
    lv_obj_set_style_pad_all(_monitor, 0, 0);
    lv_obj_remove_flag(_monitor, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_monitor, LV_OBJ_FLAG_HIDDEN);

    // The title names the scan, so the status line is just the count.
    _status = lv_label_create(_monitor);
    lv_obj_set_width(_status, 76);
    lv_obj_set_pos(_status, 6, 6);
    lv_label_set_long_mode(_status, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(_status, theme::gold(), 0);
    lv_obj_set_style_text_font(_status, &lv_font_montserrat_12, 0);

    // The one control that stops a scan (BACK only navigates). Once stopped,
    // "STOPPED" takes its place.
    _stop = theme::button(_monitor, "STOP RECON", theme::gold(), &lv_font_montserrat_10, 74, 24);
    lv_obj_set_pos(_stop, 86, 0);
    lv_obj_add_event_cb(_stop, stopThunk, LV_EVENT_CLICKED, this);
    _stopped = lv_label_create(_monitor);
    lv_label_set_text(_stopped, "STOPPED");
    lv_obj_set_width(_stopped, 74);
    lv_obj_set_pos(_stopped, 86, 6);
    lv_obj_set_style_text_align(_stopped, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(_stopped, theme::muted(), 0);
    lv_obj_set_style_text_font(_stopped, &lv_font_montserrat_12, 0);
    lv_obj_add_flag(_stopped, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *clear = theme::button(_monitor, "CLEAR LOG", theme::danger(), &lv_font_montserrat_10, 72, 24);
    lv_obj_set_pos(clear, 164, 0);
    lv_obj_add_event_cb(clear, clearThunk, LV_EVENT_CLICKED, this);

    // Its own scrolling box, so the whole 40-entry history stays reachable.
    lv_obj_t *box = lv_obj_create(_monitor);
    lv_obj_set_size(box, 240, kPageHeight - 28);
    lv_obj_set_pos(box, 0, 28);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_set_scroll_dir(box, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(box, LV_SCROLLBAR_MODE_AUTO);
    _results = lv_label_create(box);
    lv_obj_set_width(_results, 228);
    lv_obj_set_pos(_results, 6, 0);
    lv_label_set_long_mode(_results, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(_results, theme::white(), 0);
    lv_obj_set_style_text_font(_results, &lv_font_montserrat_12, 0);

    // On the top layer, above whichever screen is showing; showing or
    // dismissing it never changes screens.
    _alert = lv_obj_create(lv_layer_top());
    lv_obj_set_size(_alert, 232, 220);
    lv_obj_center(_alert);
    lv_obj_set_style_bg_color(_alert, theme::background(), 0);
    lv_obj_set_style_bg_opa(_alert, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(_alert, theme::alert(), 0);
    lv_obj_set_style_border_width(_alert, 3, 0);
    lv_obj_set_style_radius(_alert, 10, 0);
    lv_obj_set_style_pad_all(_alert, 6, 0);
    lv_obj_remove_flag(_alert, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_alert, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *alertTitle = lv_label_create(_alert);
    lv_label_set_text(alertTitle, "RECON ALERT");
    lv_obj_set_width(alertTitle, 216);
    lv_obj_align(alertTitle, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_set_style_text_align(alertTitle, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(alertTitle, theme::alert(), 0);
    lv_obj_set_style_text_font(alertTitle, &lv_font_montserrat_20, 0);
    _alertText = lv_label_create(_alert);
    lv_obj_set_size(_alertText, 216, 110);
    lv_obj_align(_alertText, LV_ALIGN_TOP_MID, 0, 32);
    lv_label_set_long_mode(_alertText, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(_alertText, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(_alertText, theme::white(), 0);
    lv_obj_set_style_text_font(_alertText, &lv_font_montserrat_14, 0);
    lv_obj_t *dismiss = lv_button_create(_alert);
    lv_obj_set_size(dismiss, 140, 38);
    lv_obj_align(dismiss, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_style_bg_color(dismiss, theme::gold(), 0);
    lv_obj_add_event_cb(dismiss, dismissThunk, LV_EVENT_CLICKED, this);
    lv_obj_t *dismissLabel = lv_label_create(dismiss);
    lv_label_set_text(dismissLabel, "DISMISS");
    lv_obj_set_style_text_color(dismissLabel, theme::background(), 0);
    lv_obj_set_style_text_font(dismissLabel, &lv_font_montserrat_16, 0);
    lv_obj_center(dismissLabel);
}

lv_obj_t *ReconScreen::createPage()
{
    lv_obj_t *page = lv_obj_create(_screen);
    lv_obj_set_size(page, 240, kPageHeight);
    lv_obj_set_pos(page, 0, kTop);
    lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_style_pad_all(page, 4, 0);
    lv_obj_set_style_pad_row(page, 5, 0);
    lv_obj_set_flex_flow(page, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(page, LV_DIR_VER);
    return page;
}

void ReconScreen::addRow(lv_obj_t *page, size_t &index, ReconTarget target, const char *title,
                         lv_color_t border, lv_color_t text, bool opensGroup)
{
    if (index >= sizeof(_contexts) / sizeof(_contexts[0])) return;
    _contexts[index] = {this, target, opensGroup};
    lv_obj_t *row = theme::button(page, title, border, &lv_font_montserrat_14, 224, 30);
    lv_obj_set_style_text_color(lv_obj_get_child(row, 0), text, 0);
    lv_obj_add_event_cb(row, rowThunk, LV_EVENT_CLICKED, &_contexts[index]);
    ++index;
}

void ReconScreen::buildPages()
{
    size_t index = 0;
    _menu = createPage();
    addRow(_menu, index, ReconTarget::All, recon::detectorName(ReconTarget::All), theme::gold(),
           theme::gold(), false);
    for (size_t i = 0; i < kGroupCount; ++i) {
        addRow(_menu, index, kGroups[i], recon::detectorName(kGroups[i]), theme::blue(), theme::blue(), true);
    }

    for (size_t i = 0; i < kGroupCount; ++i) {
        _groupPages[i] = createPage();
        lv_obj_add_flag(_groupPages[i], LV_OBJ_FLAG_HIDDEN);
        addRow(_groupPages[i], index, kGroups[i], recon::detectorName(ReconTarget::All), theme::gold(),
               theme::gold(), false);
        size_t count = 0;
        const ReconTarget *members = recon::groupMembers(kGroups[i], count);
        for (size_t j = 0; j < count; ++j) {
            addRow(_groupPages[i], index, members[j], recon::detectorName(members[j]), theme::teal(),
                   theme::white(), false);
        }
    }
}

void ReconScreen::show()
{
    _openGroup = ReconTarget::None;
    _monitorShown = false;
    showLevel(ReconTarget::None);
    lv_screen_load(_screen);
}

void ReconScreen::showMonitor(ReconTarget target)
{
    if (!_app) return;
    _openGroup = ReconTarget::None;
    lv_screen_load(_screen);
    const ReconState &state = _app->recon().state;
    select(state.monitoring ? state.selected : target);
}

void ReconScreen::showLevel(ReconTarget group)
{
    lv_obj_add_flag(_monitor, LV_OBJ_FLAG_HIDDEN);
    for (size_t i = 0; i < kGroupCount; ++i) lv_obj_add_flag(_groupPages[i], LV_OBJ_FLAG_HIDDEN);
    for (size_t i = 0; i < kGroupCount; ++i) {
        if (kGroups[i] == group) {
            lv_obj_add_flag(_menu, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(_groupPages[i], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(_title, recon::detectorName(group));
            return;
        }
    }
    _openGroup = ReconTarget::None;
    lv_obj_remove_flag(_menu, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(_title, "RECON");
}

void ReconScreen::select(ReconTarget target)
{
    if (!_app) return;
    // The scan already running is shown, not restarted (its rotation and
    // timing carry on); any other selection starts.
    const ReconState &state = _app->recon().state;
    if (!state.monitoring || state.selected != target) _app->reconStart(target);
    lv_obj_add_flag(_menu, LV_OBJ_FLAG_HIDDEN);
    for (size_t i = 0; i < kGroupCount; ++i) lv_obj_add_flag(_groupPages[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(_monitor, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(_title, recon::detectorName(target));
    _monitorShown = true;
    renderMonitor();
}

void ReconScreen::render()
{
    if (!_app) return;
    const ReconSnapshot &r = _app->recon();
    if (r.state.alertPending && r.state.lastEventId != _alertShownFor) renderAlert();
    if (lv_screen_active() != _screen) return;
    if (_monitorShown) renderMonitor();
}

void ReconScreen::renderMonitor()
{
    const ReconSnapshot &r = _app->recon();
    char status[64];
    snprintf(status, sizeof(status), "%u found.", static_cast<unsigned>(r.count));
    lv_label_set_text(_status, status);
    if (r.state.monitoring) {
        lv_obj_remove_flag(_stop, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_stopped, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(_stop, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(_stopped, LV_OBJ_FLAG_HIDDEN);
    }
    if (r.count == 0) {
        lv_label_set_text(_results, "No activity detected.");
        return;
    }
    size_t used = 0;
    gResults[0] = '\0';
    // Newest first, so a new detection is visible without scrolling.
    for (int i = static_cast<int>(r.count) - 1; i >= 0 && used + 1 < sizeof(gResults); --i) {
        text::formatEvent(r.events[i], gResults + used, sizeof(gResults) - used);
        used += strlen(gResults + used);
    }
    lv_label_set_text(_results, gResults);
}

void ReconScreen::renderAlert()
{
    const ReconSnapshot &r = _app->recon();
    if (r.count == 0) return;
    const MonitorEvent &d = r.events[r.count - 1];
    char textBuf[160];
    snprintf(textBuf, sizeof(textBuf), "%s  [%s]\n%s\n%s\n%d dBm", recon::detectorName(d.detector),
             recon::confidenceLabel(d.confidence), d.detail, d.sourceId, static_cast<int>(d.rssi));
    lv_label_set_text(_alertText, textBuf);
    _alertShownFor = r.state.lastEventId;
    lv_obj_remove_flag(_alert, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(_alert);
}

void ReconScreen::backThunk(lv_event_t *event)
{
    auto *self = static_cast<ReconScreen *>(lv_event_get_user_data(event));
    if (!self || !self->_app) return;
    // From the monitor, straight to the face with the scan still running
    // (STOP RECON stops it). From a group, the top level; from the top
    // level, the face.
    if (self->_monitorShown) {
        self->_monitorShown = false;
        self->_openGroup = ReconTarget::None;
        self->showLevel(ReconTarget::None);
        self->_app->showHome();
        return;
    }
    if (self->_openGroup != ReconTarget::None) {
        self->_openGroup = ReconTarget::None;
        self->showLevel(ReconTarget::None);
        return;
    }
    self->_app->showHome();
}

void ReconScreen::rowThunk(lv_event_t *event)
{
    auto *c = static_cast<ButtonContext *>(lv_event_get_user_data(event));
    if (!c || !c->screen) return;
    if (c->opensGroup) {
        c->screen->_openGroup = c->target;
        c->screen->showLevel(c->target);
    } else {
        c->screen->select(c->target);
    }
}

void ReconScreen::dismissThunk(lv_event_t *event)
{
    auto *self = static_cast<ReconScreen *>(lv_event_get_user_data(event));
    if (!self || !self->_app) return;
    self->_app->acknowledgeAlert();
    lv_obj_add_flag(self->_alert, LV_OBJ_FLAG_HIDDEN);
}

void ReconScreen::clearThunk(lv_event_t *event)
{
    auto *self = static_cast<ReconScreen *>(lv_event_get_user_data(event));
    if (!self || !self->_app) return;
    self->_app->reconClear();
    self->renderMonitor();
}

void ReconScreen::stopThunk(lv_event_t *event)
{
    auto *self = static_cast<ReconScreen *>(lv_event_get_user_data(event));
    if (!self || !self->_app) return;
    self->_app->reconStop();
    self->renderMonitor();
}

} // namespace twatch_s3plus
} // namespace layertime
