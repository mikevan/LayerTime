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

// Optional "Squachify" watch-face logo, read from the SD card at
// A:/assets/squach.png rather than baked into flash. The art is opt-in and
// belongs to the SquachWatch author (used with permission), so keeping it on
// the card means it can be swapped or removed without reflashing.
//
// The PNG is decoded ONCE into an RGB565 canvas at create() time. LilyGoLib
// builds LVGL with LV_CACHE_DEF_SIZE=0, so an lv_image pointed straight at
// the file would re-decode on every redraw of that region - and the watch
// face labels overlap this box, so that would mean a full decode per clock
// tick. The canvas holds finished pixels instead. Same trick as MapView.
//
// Export the PNG already sized to fit the box (320 px tall); nothing is
// scaled at runtime, it is only centred.
class SquachLogo {
public:
    // Returns false if the card is absent or the file will not decode, which
    // is the normal case - the caller then keeps showing the owl.
    bool create(lv_obj_t *parent, int x, int y, int width, int height);
    bool ok() const { return _canvas != nullptr; }
    void setHidden(bool hidden);

private:
    lv_obj_t *_canvas = nullptr;
    lv_draw_buf_t *_buffer = nullptr;
};
