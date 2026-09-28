// TEST-ONLY stand-in for LVGL. Never part of the firmware build.
//
// This is a fake, not a mock of LVGL's rendering. It keeps just enough of an
// object tree for characterization tests to read what a screen put on it:
// label text, hidden flags, textarea text, user data, and click callbacks.
// Everything purely visual (styles, sizes, positions, fonts) is accepted and
// ignored.
//
// Text formatting uses the C library's vsnprintf. On the watch, LVGL 9.4's
// builtin printf formats instead, with float support switched on by
// LilyGoLib's lv_conf.h (LV_USE_FLOAT 1). The two can disagree only when a
// value sits exactly on a rounding boundary, so tests that check formatted
// decimals must use inputs that are not on one.

#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <string>
#include <vector>

// ---------------------------------------------------------------- types

struct lv_color_t {
    uint32_t hex = 0;
};
inline lv_color_t lv_color_hex(uint32_t c) { return lv_color_t{c}; }

struct lv_font_t {
    int size;
};
inline const lv_font_t lv_font_montserrat_12{12};
inline const lv_font_t lv_font_montserrat_14{14};
inline const lv_font_t lv_font_montserrat_16{16};
inline const lv_font_t lv_font_montserrat_18{18};
inline const lv_font_t lv_font_montserrat_20{20};
inline const lv_font_t lv_font_montserrat_24{24};
inline const lv_font_t lv_font_montserrat_28{28};
inline const lv_font_t lv_font_montserrat_32{32};
inline const lv_font_t lv_font_montserrat_48{48};

using lv_text_align_t = uint8_t;
constexpr lv_text_align_t LV_TEXT_ALIGN_LEFT = 1;
constexpr lv_text_align_t LV_TEXT_ALIGN_CENTER = 2;
constexpr lv_text_align_t LV_TEXT_ALIGN_RIGHT = 3;

constexpr uint8_t LV_OPA_TRANSP = 0;
constexpr uint8_t LV_OPA_COVER = 255;

constexpr uint32_t LV_OBJ_FLAG_HIDDEN = 1u << 0;
constexpr uint32_t LV_OBJ_FLAG_CLICKABLE = 1u << 1;
constexpr uint32_t LV_OBJ_FLAG_SCROLLABLE = 1u << 4;

constexpr uint32_t LV_STATE_FOCUSED = 1u << 1;

constexpr int LV_LABEL_LONG_WRAP = 0;
constexpr int LV_FLEX_FLOW_COLUMN = 1;
constexpr int LV_FLEX_FLOW_ROW_WRAP = 4;
constexpr int LV_DIR_VER = 12;
constexpr int LV_SCROLLBAR_MODE_AUTO = 3;
constexpr int LV_ANIM_OFF = 0;
constexpr int32_t LV_SIZE_CONTENT = 2001;
constexpr int32_t LV_COORD_MAX = 536870911;
#define LV_H(x) (x)

enum lv_event_code_t { LV_EVENT_ALL = 0, LV_EVENT_PRESSED = 1, LV_EVENT_CLICKED = 10 };

enum lv_indev_type_t { LV_INDEV_TYPE_NONE = 0, LV_INDEV_TYPE_POINTER = 1 };

struct lv_event_t;
using lv_event_cb_t = void (*)(lv_event_t *);

enum class FakeKind : uint8_t { Object, Label, Button, Textarea, Keyboard };

struct lv_obj_t {
    FakeKind kind = FakeKind::Object;
    lv_obj_t *parent = nullptr;
    std::vector<lv_obj_t *> children;
    std::string text;
    uint32_t flags = 0;
    uint32_t states = 0;
    void *userData = nullptr;
    struct Callback {
        lv_event_cb_t cb;
        lv_event_code_t code;
        void *userData;
    };
    std::vector<Callback> callbacks;
};

struct lv_event_t {
    lv_obj_t *target;
    lv_event_code_t code;
    void *userData;
};

struct lv_indev_t {};
struct lv_layer_t {};
struct lv_draw_buf_t {};

// ---------------------------------------------------------------- object tree

namespace fake_lv {

inline lv_obj_t *g_active = nullptr;
inline std::vector<lv_obj_t *> g_objects;  // every live object

inline lv_obj_t *make(lv_obj_t *parent, FakeKind kind)
{
    auto *o = new lv_obj_t();
    o->kind = kind;
    o->parent = parent;
    if (parent) parent->children.push_back(o);
    g_objects.push_back(o);
    return o;
}

inline void destroy(lv_obj_t *o)
{
    for (lv_obj_t *c : o->children) destroy(c);
    g_objects.erase(std::remove(g_objects.begin(), g_objects.end(), o), g_objects.end());
    delete o;
}

// Visible on screen: neither it nor any ancestor is hidden.
inline bool visible(const lv_obj_t *o)
{
    for (; o != nullptr; o = o->parent)
        if (o->flags & LV_OBJ_FLAG_HIDDEN) return false;
    return true;
}

inline bool within(const lv_obj_t *o, const lv_obj_t *root)
{
    for (; o != nullptr; o = o->parent)
        if (o == root) return true;
    return false;
}

// Visible labels on the active screen, in creation order.
inline std::vector<lv_obj_t *> visibleLabels()
{
    std::vector<lv_obj_t *> out;
    for (lv_obj_t *o : g_objects)
        if (o->kind == FakeKind::Label && within(o, g_active) && visible(o)) out.push_back(o);
    return out;
}

inline lv_obj_t *findVisibleLabel(const std::string &text)
{
    for (lv_obj_t *o : visibleLabels())
        if (o->text == text) return o;
    return nullptr;
}

inline lv_obj_t *findVisibleLabelStartingWith(const std::string &prefix)
{
    for (lv_obj_t *o : visibleLabels())
        if (o->text.compare(0, prefix.size(), prefix) == 0) return o;
    return nullptr;
}

// Visible textarea on the active screen.
inline lv_obj_t *findVisibleTextarea()
{
    for (lv_obj_t *o : g_objects)
        if (o->kind == FakeKind::Textarea && within(o, g_active) && visible(o)) return o;
    return nullptr;
}

// Fires every callback registered on `o` for `code`, as a press would.
inline void fire(lv_obj_t *o, lv_event_code_t code)
{
    if (o == nullptr) return;
    const std::vector<lv_obj_t::Callback> callbacks = o->callbacks;
    for (const lv_obj_t::Callback &c : callbacks) {
        if (c.code != code && c.code != LV_EVENT_ALL) continue;
        lv_event_t e{o, code, c.userData};
        c.cb(&e);
    }
}

// Clicks the nearest clickable ancestor of a label, which is how a finger
// on a button's text reaches the button.
inline void click(lv_obj_t *label)
{
    for (lv_obj_t *o = label; o != nullptr; o = o->parent) {
        if (!o->callbacks.empty()) {
            fire(o, LV_EVENT_CLICKED);
            return;
        }
    }
}

inline void reset()
{
    std::vector<lv_obj_t *> roots;
    for (lv_obj_t *o : g_objects)
        if (o->parent == nullptr) roots.push_back(o);
    for (lv_obj_t *r : roots) destroy(r);
    g_active = nullptr;
}

} // namespace fake_lv

// ---------------------------------------------------------------- creation

inline lv_obj_t *lv_obj_create(lv_obj_t *parent) { return fake_lv::make(parent, FakeKind::Object); }
inline lv_obj_t *lv_label_create(lv_obj_t *parent) { return fake_lv::make(parent, FakeKind::Label); }
inline lv_obj_t *lv_button_create(lv_obj_t *parent)
{
    lv_obj_t *o = fake_lv::make(parent, FakeKind::Button);
    o->flags |= LV_OBJ_FLAG_CLICKABLE;
    return o;
}
inline lv_obj_t *lv_textarea_create(lv_obj_t *parent) { return fake_lv::make(parent, FakeKind::Textarea); }
inline lv_obj_t *lv_keyboard_create(lv_obj_t *parent) { return fake_lv::make(parent, FakeKind::Keyboard); }

inline void lv_obj_clean(lv_obj_t *o)
{
    if (o == nullptr) return;
    std::vector<lv_obj_t *> kids = o->children;
    o->children.clear();
    for (lv_obj_t *c : kids) fake_lv::destroy(c);
}

// ---------------------------------------------------------------- state that tests read

inline void lv_label_set_text(lv_obj_t *o, const char *text)
{
    if (o) o->text = text ? text : "";
}

inline void lv_label_set_text_fmt(lv_obj_t *o, const char *fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (o) o->text = buf;
}

inline void lv_textarea_set_text(lv_obj_t *o, const char *text)
{
    if (o) o->text = text ? text : "";
}
inline void lv_textarea_add_text(lv_obj_t *o, const char *text)
{
    if (o && text) o->text += text;
}
inline const char *lv_textarea_get_text(const lv_obj_t *o) { return o ? o->text.c_str() : nullptr; }

inline void lv_obj_add_flag(lv_obj_t *o, uint32_t f) { if (o) o->flags |= f; }
inline void lv_obj_remove_flag(lv_obj_t *o, uint32_t f) { if (o) o->flags &= ~f; }
inline bool lv_obj_has_flag(const lv_obj_t *o, uint32_t f) { return o && (o->flags & f) == f; }
inline void lv_obj_add_state(lv_obj_t *o, uint32_t s) { if (o) o->states |= s; }

inline void lv_obj_set_user_data(lv_obj_t *o, void *d) { if (o) o->userData = d; }
inline void *lv_obj_get_user_data(const lv_obj_t *o) { return o ? o->userData : nullptr; }

inline void lv_obj_add_event_cb(lv_obj_t *o, lv_event_cb_t cb, lv_event_code_t code, void *userData)
{
    if (o) o->callbacks.push_back({cb, code, userData});
}
inline void *lv_event_get_user_data(lv_event_t *e) { return e ? e->userData : nullptr; }
inline lv_obj_t *lv_event_get_target_obj(lv_event_t *e) { return e ? e->target : nullptr; }

inline void lv_screen_load(lv_obj_t *screen) { fake_lv::g_active = screen; }
inline lv_obj_t *lv_screen_active() { return fake_lv::g_active; }

// ---------------------------------------------------------------- accepted and ignored
// Visual-only calls. Arguments are taken as-is and discarded.

#define FAKE_LV_IGNORE(name) \
    template <class... A> inline void name(A...) {}

FAKE_LV_IGNORE(lv_obj_set_size)
FAKE_LV_IGNORE(lv_obj_set_pos)
FAKE_LV_IGNORE(lv_obj_set_width)
FAKE_LV_IGNORE(lv_obj_set_height)
FAKE_LV_IGNORE(lv_obj_center)
FAKE_LV_IGNORE(lv_obj_set_style_bg_opa)
FAKE_LV_IGNORE(lv_obj_set_style_bg_color)
FAKE_LV_IGNORE(lv_obj_set_style_border_width)
FAKE_LV_IGNORE(lv_obj_set_style_border_color)
FAKE_LV_IGNORE(lv_obj_set_style_text_color)
FAKE_LV_IGNORE(lv_obj_set_style_text_font)
FAKE_LV_IGNORE(lv_obj_set_style_text_align)
FAKE_LV_IGNORE(lv_obj_set_style_pad_all)
FAKE_LV_IGNORE(lv_obj_set_style_pad_row)
FAKE_LV_IGNORE(lv_obj_set_style_pad_column)
FAKE_LV_IGNORE(lv_obj_set_style_radius)
FAKE_LV_IGNORE(lv_obj_set_flex_flow)
FAKE_LV_IGNORE(lv_obj_set_scroll_dir)
FAKE_LV_IGNORE(lv_obj_set_scrollbar_mode)
FAKE_LV_IGNORE(lv_obj_scroll_to_y)
FAKE_LV_IGNORE(lv_label_set_long_mode)
FAKE_LV_IGNORE(lv_textarea_set_one_line)
FAKE_LV_IGNORE(lv_textarea_set_max_length)
FAKE_LV_IGNORE(lv_textarea_set_placeholder_text)
FAKE_LV_IGNORE(lv_keyboard_set_textarea)
FAKE_LV_IGNORE(lv_display_trigger_activity)
FAKE_LV_IGNORE(lv_indev_add_event_cb)

inline void lv_timer_handler() {}
inline uint32_t lv_display_get_inactive_time(void *) { return 0; }
inline lv_indev_t *lv_indev_get_next(lv_indev_t *) { return nullptr; }
inline lv_indev_type_t lv_indev_get_type(lv_indev_t *) { return LV_INDEV_TYPE_NONE; }
inline lv_obj_t *lv_indev_get_active_obj() { return nullptr; }
inline void *lv_malloc(size_t n) { return malloc(n); }
