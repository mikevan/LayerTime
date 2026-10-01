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

#pragma once

// The S3 Plus Recon screen (240 x 240): the T-Ultra's Recon menu
// (src/platform/twatch_ultra/ui/ReconScreen) laid out again for a 240 x 240
// display, reading Recon state from the S3PlusApp snapshot and sending Recon
// commands through it.
//
// Same structure and the same row names as the Ultra (the core's
// ReconSelection names): a top level of "ALL" and the three group rows
// (blue opens a group, gold or teal starts a scan), one page per group with
// "ALL" and each member, a monitor page with the results, and an alert
// overlay on LVGL's top layer so it shows above any screen. Same labels as
// the Ultra too: "CLEAR LOG" and "DISMISS".
//
// Intentional difference from the Ultra: BACK only navigates. On the monitor
// page it goes straight to the watch face and the scan keeps running, so the
// face can show "THREATS / RECON"; on a group or the top level it steps back
// one level. "STOP RECON" on the monitor page is the one control that stops
// a scan. (On the Ultra, BACK from the monitor stops the scan, so the face
// never sees a manual scan running.) Early warning is switched in Settings,
// as on the Ultra.
//
// Every page is built once in create() and then shown or hidden, never
// rebuilt: a row that rebuilt the list it lives in would delete the button
// whose click is still being dispatched (the Ultra's lesson).

#include <lvgl.h>
#include <stddef.h>
#include <stdint.h>

#include "core/model/MonitorEvent.h"

namespace layertime {
namespace twatch_s3plus {

class S3PlusApp;

class ReconScreen {
public:
    void create(S3PlusApp *app);
    void show();
    // Straight to a monitor page: the home screen's THREATS block. With a scan
    // running, that scan's monitor (nothing restarts); otherwise `target`
    // starts.
    void showMonitor(ReconTarget target);
    // Every refresh, whichever screen is showing: the alert overlay must be
    // able to appear over any of them.
    void render();

private:
    struct ButtonContext {
        ReconScreen *screen = nullptr;
        ReconTarget target = ReconTarget::None;
        bool opensGroup = false;
    };

    static void backThunk(lv_event_t *event);
    static void rowThunk(lv_event_t *event);
    static void dismissThunk(lv_event_t *event);
    static void clearThunk(lv_event_t *event);
    static void stopThunk(lv_event_t *event);

    lv_obj_t *createPage();
    void addRow(lv_obj_t *page, size_t &index, ReconTarget target, const char *title, lv_color_t border,
                lv_color_t text, bool opensGroup);
    void buildPages();
    void showLevel(ReconTarget group);
    void select(ReconTarget target);
    void renderMonitor();
    void renderAlert();

    S3PlusApp *_app = nullptr;
    lv_obj_t *_screen = nullptr;
    lv_obj_t *_title = nullptr;
    lv_obj_t *_menu = nullptr;
    lv_obj_t *_groupPages[3] = {nullptr, nullptr, nullptr};
    lv_obj_t *_monitor = nullptr;
    lv_obj_t *_status = nullptr;
    lv_obj_t *_stop = nullptr;
    lv_obj_t *_stopped = nullptr;
    lv_obj_t *_results = nullptr;
    lv_obj_t *_alert = nullptr;
    lv_obj_t *_alertText = nullptr;

    ReconTarget _openGroup = ReconTarget::None;
    bool _monitorShown = false;
    // 4 top-level rows + 5 + 4 + 6 group rows (each group page has its own
    // "ALL" plus its members) = 19 today; headroom to 24. addRow() drops rows past
    // the end rather than overrunning.
    ButtonContext _contexts[24];
    uint32_t _alertShownFor = 0;
};

} // namespace twatch_s3plus
} // namespace layertime
