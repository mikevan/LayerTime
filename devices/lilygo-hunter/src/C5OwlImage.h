// LayerTime - passive early-warning firmware for the LILYGO T-Dongle-C5.
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

// The LayerTime owl, rasterised for the C5's 160 x 80 status LCD. Generated
// into C5OwlImage.cpp by tools/c5_owl_image.py from the T-Ultra's SVG.

#include <lvgl.h>

#include "BringUpLogic.h"

namespace layertime {
namespace tdongle_c5 {

constexpr int kOwlImageSize = 56;
extern const lv_image_dsc_t kOwlImage;

// The indicator areas (BringUpLogic.h), measured on the generated pixels;
// recheck them whenever the owl is regenerated. As the viewer sees the
// screen, both are on the right half, which is green. The eye: rows 12 to
// 22, columns 32 to 46. The lens of the glasses directly below it: rows 23
// to 30, columns 30 to 46; the red bridge between the lenses ends at column
// 29. Only green pixels inside a box are recoloured.
constexpr PixelBox kOwlEyeBox{12, 22, 32, 46};
constexpr PixelBox kOwlLensBox{23, 30, 30, 46};

} // namespace tdongle_c5
} // namespace layertime
