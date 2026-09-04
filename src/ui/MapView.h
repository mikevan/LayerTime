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
#include <stdint.h>

// Offline slippy map drawn from PNG tiles on the SD card, laid out the way
// Meshtastic's device UI (MUI) expects them so the same tile packs work:
//   /maps/<style>/<z>/<x>/<y>.png   256x256, 8-bit palette PNG
// (meshtastic/device-ui maps/README.md). The style folder is auto-detected
// as the first directory under /maps.
//
// Rendering goes into one RGB565 canvas in PSRAM (LilyGoLib routes
// lv_malloc to ps_malloc) via LVGL's own draw pipeline, so a tile is
// decoded straight from the file by lodepng through the POSIX fs driver
// ('A:' -> /sd). Decoding ~9 tiles takes a noticeable fraction of a second,
// so the canvas is only re-rendered when the view changes - during a drag
// the finished canvas is simply moved under the finger, and the real
// re-render happens on release.
class MapView {
public:
    struct Marker {
        double latitude = 0.0;
        double longitude = 0.0;
        char label[8] = {0};
        lv_color_t color;
        bool emphasised = false; // drawn larger, e.g. the watch itself
    };
    static constexpr uint8_t kMaxMarkers = 40;
    static constexpr int kTileSize = 256;
    static constexpr uint8_t kMinZoom = 1;
    static constexpr uint8_t kMaxZoom = 20;

    // Builds the canvas and control buttons inside `parent` (a page
    // container). The canvas occupies (0, top) to (width, top + height).
    void create(lv_obj_t *parent, int top, int width, int height);

    // Rebuilds the marker set; nothing is drawn until render() is called.
    void clearMarkers();
    bool addMarker(double latitude, double longitude, const char *label, lv_color_t color, bool emphasised);

    void setCenter(double latitude, double longitude);
    void setZoom(uint8_t zoom);
    uint8_t zoom() const { return _zoom; }
    bool hasTiles() const { return _style[0] != '\0'; }

    // Redraws tiles and markers for the current view. Safe to call when
    // the page is hidden; it just costs the decode time.
    void render();
    // Only redraws if something changed since the last render (markers
    // updated, view moved).
    void renderIfDirty();
    void markDirty() { _dirty = true; }

    // Hook for the CENTER button: the owner decides what "center" means.
    using CenterCallback = void (*)(void *userData);
    void setCenterCallback(CenterCallback callback, void *userData);

private:
    bool detectStyle();
    bool tileExists(uint8_t zoom, int32_t x, int32_t y, char *path, size_t pathSize) const;
    void worldPixel(double latitude, double longitude, double &px, double &py) const;
    void drawMarkers(lv_layer_t &layer, int32_t left, int32_t top);
    void drawStatus();

    static void pressingThunk(lv_event_t *event);
    static void releasedThunk(lv_event_t *event);
    static void zoomInThunk(lv_event_t *event);
    static void zoomOutThunk(lv_event_t *event);
    static void centerThunk(lv_event_t *event);

    lv_obj_t *_canvas = nullptr;
    lv_draw_buf_t *_buffer = nullptr;
    lv_obj_t *_status = nullptr;
    lv_obj_t *_notice = nullptr;
    int _canvasX = 0;
    int _canvasY = 0;
    int _width = 0;
    int _height = 0;

    char _style[24] = {0};
    uint8_t _zoom = 12;
    // View center in Web Mercator world pixels at the current zoom.
    double _centerPx = 0.0;
    double _centerPy = 0.0;
    bool _hasCenter = false;
    int32_t _dragX = 0;
    int32_t _dragY = 0;
    bool _dirty = true;
    uint8_t _tilesDrawn = 0;
    uint8_t _tilesMissing = 0;

    Marker _markers[kMaxMarkers];
    uint8_t _markerCount = 0;

    CenterCallback _centerCallback = nullptr;
    void *_centerUserData = nullptr;
};
