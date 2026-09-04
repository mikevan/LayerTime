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

namespace Theme {
    inline lv_color_t background() { return lv_color_hex(0x050A0A); }
    inline lv_color_t gold()       { return lv_color_hex(0xD99A24); }
    inline lv_color_t teal()       { return lv_color_hex(0x1CB7B0); }
    // Group rows in the Recon menu. Deliberately a true blue rather than a
    // near-teal, so "opens a group" and "starts a scan" are never confused
    // at a glance on the near-black background.
    inline lv_color_t blue()       { return lv_color_hex(0x3D8BF0); }
    inline lv_color_t green()      { return lv_color_hex(0x63E06B); }
    inline lv_color_t white()      { return lv_color_hex(0xE7ECEB); }
    inline lv_color_t muted()      { return lv_color_hex(0x657474); }
    inline lv_color_t danger()     { return lv_color_hex(0xE0524A); }
}
