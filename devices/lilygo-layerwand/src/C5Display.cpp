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

// This file belongs to the tdongle_c5 build environments only
// (devices/lilygo-layerwand/platformio.ini). The guard below dates from when
// every environment compiled all of src/, and is kept as a safety net.
#if defined(LAYERTIME_TARGET_TDONGLE_C5)

#include "C5Display.h"
#include "C5OwlImage.h"
#include "TDongleC5Pins.h"

#include <Arduino.h>
#include <SPI.h>
#include <lvgl.h>
#include <string.h>

namespace layertime {
namespace tdongle_c5 {

namespace {

// 20 MHz, the clock LILYGO's own ST7735 driver uses on this board.
const SPISettings kLcdSpi(20000000, MSBFIRST, SPI_MODE0);

// One partial-render buffer of 20 landscape rows, RGB565.
constexpr uint32_t kBufferRows = 20;
uint8_t gDrawBuffer[pins::kLcdNativeHeight * kBufferRows * 2];

lv_obj_t *gLines[C5Display::kLineCount] = {};

// Every LCD transfer; the LED shares this bus and is scrambled by each one.
uint32_t gTransfers = 0;

// The owl the screen shows: a RAM copy of the generated image, with the
// indicators drawn into it (setIndicators).
uint8_t gOwlPixels[kOwlImageSize * kOwlImageSize * 2];
lv_image_dsc_t gOwlDsc;
lv_obj_t *gOwl = nullptr;
bool gIndicatorsSet = false;
LinkIndicator gLinkIndicator = LinkIndicator::NoWatch;
MeshIndicator gMeshIndicator = MeshIndicator::NotConnected;

void drawIndicators()
{
    memcpy(gOwlPixels, kOwlImage.data, sizeof(gOwlPixels));
    if (gIndicatorsSet) {
        recolorOwlIndicator(gOwlPixels, kOwlImageSize, kOwlEyeBox, linkIndicatorColor(gLinkIndicator));
        recolorOwlIndicator(gOwlPixels, kOwlImageSize, kOwlLensBox, meshIndicatorColor(gMeshIndicator));
    }
    if (gOwl != nullptr) {
        lv_obj_invalidate(gOwl);
    }
}

// Layout, landscape 160 x 80: the owl on the left, status text on the right.
// The owl is 56 x 56, centred vertically; the text column starts at x = 62
// and runs to the right edge, 96 px, which fits 14 to 15 characters of
// Montserrat 12 per line at a 16 px pitch.
constexpr int32_t kOwlX = 2;
constexpr int32_t kOwlY = (pins::kLcdNativeWidth - kOwlImageSize) / 2; // 12
constexpr int32_t kTextX = 62;
constexpr int32_t kTextWidth = pins::kLcdNativeHeight - kTextX - 2;      // 96
constexpr int32_t kLinePitch = 16;

uint32_t tickMs() { return millis(); }

void transfer(const uint8_t *cmd, size_t cmdSize, const uint8_t *param, size_t paramSize)
{
    SPI.beginTransaction(kLcdSpi);
    digitalWrite(pins::kLcdCs, LOW);
    digitalWrite(pins::kLcdDc, LOW);
    SPI.writeBytes(cmd, cmdSize);
    if (paramSize > 0) {
        digitalWrite(pins::kLcdDc, HIGH);
        SPI.writeBytes(param, paramSize);
    }
    digitalWrite(pins::kLcdCs, HIGH);
    SPI.endTransaction();
    ++gTransfers;
}

void sendCommand(lv_display_t *, const uint8_t *cmd, size_t cmdSize, const uint8_t *param,
                 size_t paramSize)
{
    transfer(cmd, cmdSize, param, paramSize);
}

void sendColor(lv_display_t *disp, const uint8_t *cmd, size_t cmdSize, uint8_t *param,
               size_t paramSize)
{
    // LVGL renders RGB565 little-endian; the ST7735 takes the high byte first.
    lv_draw_sw_rgb565_swap(param, paramSize / 2);
    transfer(cmd, cmdSize, param, paramSize);
    lv_display_flush_ready(disp);
}

} // namespace

bool C5Display::begin()
{
    pinMode(pins::kLcdCs, OUTPUT);
    pinMode(pins::kLcdDc, OUTPUT);
    pinMode(pins::kLcdRst, OUTPUT);
    pinMode(pins::kLcdBacklight, OUTPUT);
    digitalWrite(pins::kLcdCs, HIGH);
    setBacklight(false);

    // Hardware reset, timed as LILYGO's driver does it.
    digitalWrite(pins::kLcdRst, HIGH);
    delay(100);
    digitalWrite(pins::kLcdRst, LOW);
    delay(100);
    digitalWrite(pins::kLcdRst, HIGH);
    delay(120);

    SPI.begin(pins::kLcdSck, pins::kSdMiso, pins::kLcdMosi, -1); // MISO for the SD card (C5SdLog.h)

    lv_init();
    lv_tick_set_cb(tickMs);

    lv_display_t *disp = lv_st7735_create(pins::kLcdNativeWidth, pins::kLcdNativeHeight,
                                          LV_LCD_FLAG_BGR, sendCommand, sendColor);
    if (disp == nullptr) return false;
    lv_st7735_set_invert(disp, true);
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_90);
    // In landscape the panel's column offset lands on the row axis and the
    // row offset on the column axis. Both offsets are symmetric in the
    // controller's 132 x 162 RAM, so mirroring does not change them.
    lv_st7735_set_gap(disp, pins::kLcdRowOffset, pins::kLcdColumnOffset);
    lv_display_set_buffers(disp, gDrawBuffer, nullptr, sizeof(gDrawBuffer),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    lv_obj_t *owl = lv_image_create(screen);
    if (kOwlImage.data_size == sizeof(gOwlPixels)) {
        gOwlDsc = kOwlImage;
        gOwlDsc.data = gOwlPixels;
        drawIndicators();
        lv_image_set_src(owl, &gOwlDsc);
        gOwl = owl;
    } else {
        lv_image_set_src(owl, &kOwlImage); // no indicators
    }
    lv_obj_set_pos(owl, kOwlX, kOwlY);

    for (uint8_t i = 0; i < kLineCount; ++i) {
        gLines[i] = lv_label_create(screen);
        lv_obj_set_style_text_color(gLines[i], lv_color_white(), 0);
        lv_obj_set_style_text_font(gLines[i], &lv_font_montserrat_12, 0);
        lv_label_set_long_mode(gLines[i], LV_LABEL_LONG_MODE_CLIP);
        lv_obj_set_width(gLines[i], kTextWidth);
        lv_obj_set_pos(gLines[i], kTextX, 1 + i * kLinePitch);
        lv_label_set_text(gLines[i], "");
    }
    lv_timer_handler();
    setBacklight(true);
    return true;
}

void C5Display::setLine(uint8_t line, const char *text)
{
    if (line >= kLineCount || gLines[line] == nullptr) return;
    lv_label_set_text(gLines[line], text);
}

void C5Display::setIndicators(LinkIndicator link, MeshIndicator mesh)
{
    if (gIndicatorsSet && link == gLinkIndicator && mesh == gMeshIndicator) return;
    gIndicatorsSet = true;
    gLinkIndicator = link;
    gMeshIndicator = mesh;
    if (gOwl != nullptr) drawIndicators();
}

void C5Display::service()
{
    lv_timer_handler();
}

uint32_t C5Display::transfers() const
{
    return gTransfers;
}

void C5Display::setBacklight(bool on)
{
    const bool level = on ? pins::kLcdBacklightOnLevel : !pins::kLcdBacklightOnLevel;
    digitalWrite(pins::kLcdBacklight, level ? HIGH : LOW);
}

} // namespace tdongle_c5
} // namespace layertime

#endif
