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

#include "SquachLogo.h"

#include "Theme.h"

namespace {
// 'A:' is LVGL's POSIX filesystem letter, which lv_fs_posix maps to "/sd".
constexpr const char *kSquachPath = "A:/assets/squach.png";
}

bool SquachLogo::create(lv_obj_t *parent, int x, int y, int width, int height)
{
    if (_canvas != nullptr) return true;

    // Ask the decoder for the size first: this also tells us whether the card
    // is mounted and the file is readable, before allocating a canvas.
    lv_image_header_t header;
    if (lv_image_decoder_get_info(kSquachPath, &header) != LV_RESULT_OK) return false;

    _buffer = lv_draw_buf_create(width, height, LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
    if (_buffer == nullptr) return false;

    _canvas = lv_canvas_create(parent);
    if (_canvas == nullptr) {
        lv_draw_buf_destroy(_buffer);
        _buffer = nullptr;
        return false;
    }

    // Created lazily from render(), long after the watch face's buttons and
    // labels, and LVGL draws children in creation order - so without this the
    // logo covers the whole face. The owl does not need it: it is built first.
    lv_obj_move_to_index(_canvas, 0);
    lv_obj_set_pos(_canvas, x, y);
    lv_obj_set_size(_canvas, width, height);
    lv_canvas_set_draw_buf(_canvas, _buffer);
    // The PNG has an alpha channel, so fill with the face background first and
    // let the draw composite onto it - the canvas itself is RGB565, no alpha.
    lv_canvas_fill_bg(_canvas, Theme::background(), LV_OPA_COVER);

    lv_layer_t layer;
    lv_canvas_init_layer(_canvas, &layer);

    lv_draw_image_dsc_t image;
    lv_draw_image_dsc_init(&image);
    // A string literal has static storage duration, so the draw task can hold
    // this pointer safely until lv_canvas_finish_layer().
    image.src = kSquachPath;

    lv_area_t area;
    area.x1 = (width - static_cast<int32_t>(header.w)) / 2;
    area.y1 = (height - static_cast<int32_t>(header.h)) / 2;
    area.x2 = area.x1 + static_cast<int32_t>(header.w) - 1;
    area.y2 = area.y1 + static_cast<int32_t>(header.h) - 1;
    lv_draw_image(&layer, &image, &area);

    lv_canvas_finish_layer(_canvas, &layer);

    lv_obj_add_flag(_canvas, LV_OBJ_FLAG_HIDDEN);
    return true;
}

void SquachLogo::setHidden(bool hidden)
{
    if (_canvas == nullptr) return;
    if (hidden) lv_obj_add_flag(_canvas, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(_canvas, LV_OBJ_FLAG_HIDDEN);
}
