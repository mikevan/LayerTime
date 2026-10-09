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

#include "MapView.h"
#include "Theme.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {
constexpr int kButtonHeight = 42;
constexpr int kButtonGap = 10;
constexpr double kPi = 3.14159265358979323846;

lv_obj_t *makeMapButton(lv_obj_t *parent, const char *text, int x, int y, int w, lv_event_cb_t cb, void *userData)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, w, kButtonHeight);
    lv_obj_set_style_bg_color(button, Theme::background(), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(button, Theme::blue(), 0);
    lv_obj_set_style_border_width(button, 2, 0);
    lv_obj_set_style_radius(button, 8, 0);
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, userData);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, Theme::blue(), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_center(label);
    return button;
}
}

void MapView::create(lv_obj_t *parent, int top, int width, int height)
{
    _canvasX = 0;
    _canvasY = top;
    _width = width;
    _height = height;

    _buffer = lv_draw_buf_create(width, height, LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
    _canvas = lv_canvas_create(parent);
    lv_obj_set_pos(_canvas, _canvasX, _canvasY);
    lv_obj_set_size(_canvas, width, height);
    if (_buffer) {
        lv_canvas_set_draw_buf(_canvas, _buffer);
        lv_canvas_fill_bg(_canvas, Theme::background(), LV_OPA_COVER);
    }
    lv_obj_add_flag(_canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(_canvas, LV_OBJ_FLAG_PRESS_LOCK); // keep the drag even when the finger leaves the canvas
    lv_obj_remove_flag(_canvas, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(_canvas, pressingThunk, LV_EVENT_PRESSING, this);
    lv_obj_add_event_cb(_canvas, releasedThunk, LV_EVENT_RELEASED, this);
    lv_obj_add_event_cb(_canvas, releasedThunk, LV_EVENT_PRESS_LOST, this);

    // Zoom / status line floats over the top edge of the map.
    _status = lv_label_create(parent);
    lv_obj_set_pos(_status, 8, top + 6);
    lv_obj_set_width(_status, width - 16);
    lv_obj_set_style_text_font(_status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(_status, Theme::white(), 0);
    lv_obj_set_style_bg_color(_status, Theme::background(), 0);
    lv_obj_set_style_bg_opa(_status, LV_OPA_70, 0);
    lv_obj_set_style_pad_all(_status, 4, 0);
    lv_label_set_text(_status, "");

    // Full-width message when there is nothing to draw.
    _notice = lv_label_create(parent);
    lv_obj_set_pos(_notice, 16, top + height / 2 - 40);
    lv_obj_set_width(_notice, width - 32);
    lv_label_set_long_mode(_notice, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(_notice, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(_notice, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(_notice, Theme::gold(), 0);
    lv_obj_add_flag(_notice, LV_OBJ_FLAG_HIDDEN);

    const int buttonY = top + height + 6;
    const int buttonW = (width - 24 - 2 * kButtonGap) / 3;
    makeMapButton(parent, "ZOOM -", 12, buttonY, buttonW, zoomOutThunk, this);
    makeMapButton(parent, "ZOOM +", 12 + buttonW + kButtonGap, buttonY, buttonW, zoomInThunk, this);
    makeMapButton(parent, "CENTER", 12 + 2 * (buttonW + kButtonGap), buttonY, buttonW, centerThunk, this);

    detectStyle();
}

void MapView::setCenterCallback(CenterCallback callback, void *userData)
{
    _centerCallback = callback;
    _centerUserData = userData;
}

// ---------------------------------------------------------------- tiles

bool MapView::detectStyle()
{
    // First directory under /maps is the style (osm, topo, ...). LVGL's fs
    // layer marks directories with a leading '/' in the entry name.
    _style[0] = '\0';
    lv_fs_dir_t dir;
    if (lv_fs_dir_open(&dir, "A:/maps") != LV_FS_RES_OK) return false;
    char entry[64];
    while (lv_fs_dir_read(&dir, entry, sizeof(entry)) == LV_FS_RES_OK && entry[0] != '\0') {
        if (entry[0] == '/' && entry[1] != '.' && entry[1] != '\0') {
            snprintf(_style, sizeof(_style), "%s", entry + 1);
            break;
        }
    }
    lv_fs_dir_close(&dir);
    return _style[0] != '\0';
}

bool MapView::tileExists(uint8_t zoom, int32_t x, int32_t y, char *path, size_t pathSize) const
{
    const int32_t n = 1L << zoom;
    if (x < 0 || y < 0 || x >= n || y >= n) return false;
    snprintf(path, pathSize, "A:/maps/%s/%u/%ld/%ld.png", _style, static_cast<unsigned>(zoom),
             static_cast<long>(x), static_cast<long>(y));
    lv_fs_file_t file;
    if (lv_fs_open(&file, path, LV_FS_MODE_RD) != LV_FS_RES_OK) return false;
    lv_fs_close(&file);
    return true;
}

// Web Mercator: world pixel coordinates at the current zoom, 256 px tiles.
void MapView::worldPixel(double latitude, double longitude, double &px, double &py) const
{
    const double n = static_cast<double>(1L << _zoom) * kTileSize;
    if (latitude > 85.05112878) latitude = 85.05112878;
    if (latitude < -85.05112878) latitude = -85.05112878;
    const double latRad = latitude * kPi / 180.0;
    px = (longitude + 180.0) / 360.0 * n;
    py = (1.0 - log(tan(latRad) + 1.0 / cos(latRad)) / kPi) / 2.0 * n;
}

void MapView::setCenter(double latitude, double longitude)
{
    worldPixel(latitude, longitude, _centerPx, _centerPy);
    _hasCenter = true;
    _dirty = true;
}

void MapView::setZoom(uint8_t zoom)
{
    if (zoom < kMinZoom) zoom = kMinZoom;
    if (zoom > kMaxZoom) zoom = kMaxZoom;
    if (zoom == _zoom) return;
    // Keep the same geographic center: world pixels scale by 2 per level.
    const double scale = pow(2.0, static_cast<double>(zoom) - static_cast<double>(_zoom));
    _centerPx *= scale;
    _centerPy *= scale;
    _zoom = zoom;
    _dirty = true;
}

// ---------------------------------------------------------------- markers

void MapView::clearMarkers()
{
    _markerCount = 0;
    _dirty = true;
}

bool MapView::addMarker(double latitude, double longitude, const char *label, lv_color_t color, bool emphasised)
{
    if (_markerCount >= kMaxMarkers) return false;
    Marker &m = _markers[_markerCount++];
    m.latitude = latitude;
    m.longitude = longitude;
    m.color = color;
    m.emphasised = emphasised;
    snprintf(m.label, sizeof(m.label), "%s", label ? label : "");
    _dirty = true;
    return true;
}

void MapView::drawMarkers(lv_layer_t &layer, int32_t left, int32_t top)
{
    for (uint8_t i = 0; i < _markerCount; ++i) {
        const Marker &m = _markers[i];
        double px, py;
        worldPixel(m.latitude, m.longitude, px, py);
        const int32_t cx = static_cast<int32_t>(px) - left;
        const int32_t cy = static_cast<int32_t>(py) - top;
        if (cx < -30 || cy < -30 || cx > _width + 30 || cy > _height + 30) continue;

        const int32_t radius = m.emphasised ? 9 : 7;
        // Dark outline so the dot reads on any tile colour, then the fill.
        lv_draw_arc_dsc_t outline;
        lv_draw_arc_dsc_init(&outline);
        outline.center.x = cx;
        outline.center.y = cy;
        outline.radius = static_cast<uint16_t>(radius + 2);
        outline.width = radius + 2;
        outline.start_angle = 0;
        outline.end_angle = 360;
        outline.color = Theme::background();
        outline.opa = LV_OPA_COVER;
        lv_draw_arc(&layer, &outline);

        lv_draw_arc_dsc_t dot;
        lv_draw_arc_dsc_init(&dot);
        dot.center.x = cx;
        dot.center.y = cy;
        dot.radius = static_cast<uint16_t>(radius);
        dot.width = radius;
        dot.start_angle = 0;
        dot.end_angle = 360;
        dot.color = m.color;
        dot.opa = LV_OPA_COVER;
        lv_draw_arc(&layer, &dot);

        if (m.label[0] == '\0') continue;
        lv_draw_label_dsc_t text;
        lv_draw_label_dsc_init(&text);
        text.text = m.label;
        text.font = &lv_font_montserrat_14;
        text.color = Theme::white();
        text.align = LV_TEXT_ALIGN_LEFT;
        lv_area_t box;
        box.x1 = cx + radius + 4;
        box.y1 = cy - 9;
        box.x2 = box.x1 + 80;
        box.y2 = box.y1 + 18;
        lv_draw_label(&layer, &text, &box);
    }
}

// ---------------------------------------------------------------- render

void MapView::render()
{
    _dirty = false;
    if (_canvas == nullptr || _buffer == nullptr) return;

    if (!hasTiles()) detectStyle();
    if (!hasTiles()) {
        lv_canvas_fill_bg(_canvas, Theme::background(), LV_OPA_COVER);
        lv_label_set_text(_notice, "NO MAP TILES ON SD\n\nCopy MUI tile packs to\n/maps/<style>/z/x/y.png");
        lv_obj_remove_flag(_notice, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(_status, "");
        return;
    }
    if (!_hasCenter) {
        lv_canvas_fill_bg(_canvas, Theme::background(), LV_OPA_COVER);
        lv_label_set_text(_notice, "NO POSITION YET\n\nWaiting for a GPS fix or a node\nthat has reported one");
        lv_obj_remove_flag(_notice, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(_status, "");
        return;
    }
    lv_obj_add_flag(_notice, LV_OBJ_FLAG_HIDDEN);

    const int32_t left = static_cast<int32_t>(floor(_centerPx)) - _width / 2;
    const int32_t top = static_cast<int32_t>(floor(_centerPy)) - _height / 2;

    lv_canvas_fill_bg(_canvas, lv_color_hex(0x1A2424), LV_OPA_COVER);

    lv_layer_t layer;
    lv_canvas_init_layer(_canvas, &layer);

    _tilesDrawn = 0;
    _tilesMissing = 0;
    const int32_t firstTx = static_cast<int32_t>(floor(static_cast<double>(left) / kTileSize));
    const int32_t lastTx = static_cast<int32_t>(floor(static_cast<double>(left + _width - 1) / kTileSize));
    const int32_t firstTy = static_cast<int32_t>(floor(static_cast<double>(top) / kTileSize));
    const int32_t lastTy = static_cast<int32_t>(floor(static_cast<double>(top + _height - 1) / kTileSize));
    // The draw task keeps the src POINTER, and the tasks may only run in
    // lv_canvas_finish_layer(), so every tile needs its own path buffer.
    constexpr size_t kMaxTiles = 16; // 3x3 covers 410x382 with any offset
    static char paths[kMaxTiles][96];
    size_t slot = 0;
    for (int32_t ty = firstTy; ty <= lastTy && slot < kMaxTiles; ++ty) {
        for (int32_t tx = firstTx; tx <= lastTx && slot < kMaxTiles; ++tx) {
            lv_area_t area;
            area.x1 = tx * kTileSize - left;
            area.y1 = ty * kTileSize - top;
            area.x2 = area.x1 + kTileSize - 1;
            area.y2 = area.y1 + kTileSize - 1;
            char *path = paths[slot];
            if (!tileExists(_zoom, tx, ty, path, sizeof(paths[0]))) {
                ++_tilesMissing;
                continue;
            }
            ++slot;
            lv_draw_image_dsc_t img;
            lv_draw_image_dsc_init(&img);
            img.src = path;
            lv_draw_image(&layer, &img, &area);
            ++_tilesDrawn;
        }
    }

    drawMarkers(layer, left, top);
    lv_canvas_finish_layer(_canvas, &layer);
    drawStatus();
}

void MapView::renderIfDirty()
{
    if (_dirty) render();
}

void MapView::drawStatus()
{
    if (_tilesDrawn == 0 && _tilesMissing > 0) {
        lv_label_set_text_fmt(_status, "z%u  %s  no tiles here - try ZOOM -", static_cast<unsigned>(_zoom), _style);
    } else {
        lv_label_set_text_fmt(_status, "z%u  %s  %u node%s", static_cast<unsigned>(_zoom), _style,
                              static_cast<unsigned>(_markerCount), _markerCount == 1 ? "" : "s");
    }
}

// ---------------------------------------------------------------- input

void MapView::pressingThunk(lv_event_t *event)
{
    auto *self = static_cast<MapView *>(lv_event_get_user_data(event));
    if (!self) return;
    lv_indev_t *indev = lv_indev_active();
    if (!indev) return;
    lv_point_t vect;
    lv_indev_get_vect(indev, &vect);
    self->_dragX += vect.x;
    self->_dragY += vect.y;
    // Slide the already-rendered canvas under the finger; no decode yet.
    lv_obj_set_pos(self->_canvas, self->_canvasX + self->_dragX, self->_canvasY + self->_dragY);
}

void MapView::releasedThunk(lv_event_t *event)
{
    auto *self = static_cast<MapView *>(lv_event_get_user_data(event));
    if (!self) return;
    if (self->_dragX == 0 && self->_dragY == 0) return;
    self->_centerPx -= self->_dragX;
    self->_centerPy -= self->_dragY;
    self->_dragX = 0;
    self->_dragY = 0;
    lv_obj_set_pos(self->_canvas, self->_canvasX, self->_canvasY);
    self->render();
}

void MapView::zoomInThunk(lv_event_t *event)
{
    auto *self = static_cast<MapView *>(lv_event_get_user_data(event));
    if (!self) return;
    self->setZoom(static_cast<uint8_t>(self->_zoom + 1));
    self->render();
}

void MapView::zoomOutThunk(lv_event_t *event)
{
    auto *self = static_cast<MapView *>(lv_event_get_user_data(event));
    if (!self || self->_zoom == 0) return;
    self->setZoom(static_cast<uint8_t>(self->_zoom - 1));
    self->render();
}

void MapView::centerThunk(lv_event_t *event)
{
    auto *self = static_cast<MapView *>(lv_event_get_user_data(event));
    if (!self) return;
    if (self->_centerCallback) self->_centerCallback(self->_centerUserData);
    self->render();
}
