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

#include "S3Theme.h"

namespace layertime {
namespace twatch_s3plus {
namespace theme {

lv_obj_t *button(lv_obj_t *parent, const char *text, lv_color_t colour, const lv_font_t *font,
                 int32_t width, int32_t height)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_size(b, width, height);
    lv_obj_set_style_bg_color(b, background(), 0);
    lv_obj_set_style_border_color(b, colour, 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_radius(b, 8, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_t *label = lv_label_create(b);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, colour, 0);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_center(label);
    return b;
}

lv_obj_t *screen()
{
    lv_obj_t *s = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(s, background(), 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s, 0, 0);
    lv_obj_remove_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    return s;
}

} // namespace theme
} // namespace twatch_s3plus
} // namespace layertime
