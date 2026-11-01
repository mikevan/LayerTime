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

// The heads the C5's 160 x 80 status LCD shows (Michael, 2026-11-01): the
// Hunter cat on a Hunter, the Vixen fox on a Vixen. They replaced the owl,
// which is the Layers and PRS logo. Generated into C5LogoImages.cpp by
// tools/c5_logo_images.py from assets/hunter-logo.png and assets/vixen-logo.png.

#include <lvgl.h>

#include "BringUpLogic.h"

namespace layertime {
namespace tdongle_c5 {

constexpr int kLogoImageSize = 56;
extern const lv_image_dsc_t kHunterImage;
extern const lv_image_dsc_t kVixenImage;

// The Hunter's indicators (BringUpLogic.h), measured on the generated pixels
// (the script prints them); recheck them whenever the head is regenerated.
// The head's only green is the stripe through its right eye (as the viewer
// sees it) and down its cheek. The eye: rows 23 to 28, green in columns 35
// to 46. The cheek: rows 29 to 39, green in columns 34 to 42. Only green
// pixels inside a box are recoloured.
constexpr PixelBox kHunterEyeBox{23, 28, 34, 47};
constexpr PixelBox kHunterCheekBox{29, 39, 33, 43};

} // namespace tdongle_c5
} // namespace layertime
