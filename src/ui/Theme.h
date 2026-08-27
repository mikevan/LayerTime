#pragma once

#include <lvgl.h>

namespace Theme {
    inline lv_color_t background() { return lv_color_hex(0x050A0A); }
    inline lv_color_t gold()       { return lv_color_hex(0xD99A24); }
    inline lv_color_t teal()       { return lv_color_hex(0x1CB7B0); }
    inline lv_color_t green()      { return lv_color_hex(0x63E06B); }
    inline lv_color_t white()      { return lv_color_hex(0xE7ECEB); }
    inline lv_color_t muted()      { return lv_color_hex(0x657474); }
    inline lv_color_t danger()     { return lv_color_hex(0xE0524A); }
}
