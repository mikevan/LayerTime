#pragma once

#include <lvgl.h>

// Optional "Squatchify" watch-face logo, read from the SD card at
// A:/assets/squatch.png rather than baked into flash. The art is opt-in and
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
class SquatchLogo {
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
