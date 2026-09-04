// LayerTime - counter-intrusion and resilient-communications firmware
// for the LilyGo T-Watch Ultra.
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

#include <lvgl.h>
#include "../services/ReconService.h"

class ReconScreen {
public:
    using BackCallback = void (*)(void *userData);
    void create(ReconService *service, BackCallback backCallback, void *userData);
    // detector: None opens the detector-picker menu (default, matches the
    // RECON watch-face button). Any other value skips straight to that
    // detector's monitor page, e.g. ReconDetector::All for the THREATS box.
    void show(ReconDetector detector = ReconDetector::None);
    void render();

private:
    struct ButtonContext {
        ReconScreen *screen = nullptr;
        ReconDetector detector = ReconDetector::None;
        // True for the blue top-level group rows, which open a sub-page
        // instead of starting a scan.
        bool opensGroup = false;
    };

    static void backThunk(lv_event_t *event);
    static void detectorThunk(lv_event_t *event);
    static void dismissThunk(lv_event_t *event);
    static void clearLogThunk(lv_event_t *event);
    void selectDetector(ReconDetector detector);
    void openGroup(ReconDetector group);
    // Every page is built once in create() and then shown/hidden - never
    // rebuilt - because a group row rebuilding the list it lives in would
    // delete the button whose click is still being dispatched.
    void buildMenuPages();
    lv_obj_t *createMenuPage();
    void addMenuButton(lv_obj_t *parent, size_t &index, ReconDetector detector,
                       const char *title, lv_color_t border, lv_color_t text,
                       bool opensGroup);
    void showMenuLevel(ReconDetector group);
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
    lv_obj_t *_title = nullptr;
    // One page per group, index-aligned with kGroups in the .cpp.
    lv_obj_t *_groupPages[3] = {nullptr, nullptr, nullptr};
    // Which group's sub-page is open (None = top level). Kept across a scan
    // so BACK out of the monitor returns to the group you drilled in from.
    ReconDetector _openGroup = ReconDetector::None;
    // 4 top-level rows + 5 (TRACKERS) + 3 (COUNTER-SURVEIL) + 6
    // (COUNTER-INTRUSION) = 18 today; sized for headroom. addMenuButton()
    // drops buttons past the end rather than overrunning, so if rows go
    // missing from a sub-page this is the number to raise.
    ButtonContext _buttonContexts[24];
    uint32_t _renderedEventSerial = 0;
};
