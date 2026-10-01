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

// The S3 Plus colours: the T-Ultra's palette (src/platform/twatch_ultra/ui/
// Theme.h), copied so the two watches read the same, plus the alert red the
// Ultra's Recon alert uses.

#include <lvgl.h>

namespace layertime {
namespace twatch_s3plus {
namespace theme {

inline lv_color_t background() { return lv_color_hex(0x050A0A); }
inline lv_color_t gold() { return lv_color_hex(0xD99A24); }
inline lv_color_t teal() { return lv_color_hex(0x1CB7B0); }
// Group rows: a true blue, so "opens a group" never reads as "starts a scan".
inline lv_color_t blue() { return lv_color_hex(0x3D8BF0); }
inline lv_color_t green() { return lv_color_hex(0x63E06B); }
inline lv_color_t white() { return lv_color_hex(0xE7ECEB); }
inline lv_color_t muted() { return lv_color_hex(0x657474); }
inline lv_color_t danger() { return lv_color_hex(0xE0524A); }
inline lv_color_t alert() { return lv_color_hex(0xFF3030); }

// A bordered button on the dark background, with a centred label. Returns
// the button; the label is its only child.
lv_obj_t *button(lv_obj_t *parent, const char *text, lv_color_t colour, const lv_font_t *font,
                 int32_t width, int32_t height);

// A screen object with the dark background and no scrolling.
lv_obj_t *screen();

} // namespace theme
} // namespace twatch_s3plus
} // namespace layertime
