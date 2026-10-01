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

#include "SettingsScreen.h"

#include "../app/S3PlusApp.h"
#include "S3Theme.h"

namespace layertime {
namespace twatch_s3plus {

namespace {
constexpr int32_t kTop = 34;
constexpr int32_t kRowHeight = 30;
constexpr int32_t kButtonWidth = 162;
constexpr int32_t kValueX = 168;
constexpr int32_t kValueWidth = 64;

lv_obj_t *rowBox(lv_obj_t *page, int32_t height)
{
    lv_obj_t *box = lv_obj_create(page);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, 232, height);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);
    return box;
}

// Montserrat 12, or 10 when the title would run to the button's edge
// ("MESHTASTIC ADVERTISE").
const lv_font_t *rowFont(const char *title)
{
    lv_point_t size;
    lv_text_get_size(&size, title, &lv_font_montserrat_12, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return size.x > kButtonWidth - 20 ? &lv_font_montserrat_10 : &lv_font_montserrat_12;
}

lv_obj_t *valueLabel(lv_obj_t *box, lv_color_t colour, const char *text)
{
    lv_obj_t *l = lv_label_create(box);
    lv_obj_set_width(l, kValueWidth);
    lv_obj_set_pos(l, kValueX, 8);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(l, colour, 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
    lv_label_set_text(l, text);
    return l;
}
} // namespace

void SettingsScreen::create(S3PlusApp *app)
{
    _app = app;
    _screen = theme::screen();

    lv_obj_t *back = theme::button(_screen, "BACK", theme::gold(), &lv_font_montserrat_12, 56, 26);
    lv_obj_set_pos(back, 4, 4);
    lv_obj_add_event_cb(back, backThunk, LV_EVENT_CLICKED, this);

    lv_obj_t *title = lv_label_create(_screen);
    lv_obj_set_width(title, 172);
    lv_obj_set_pos(title, 64, 9);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(title, theme::gold(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_label_set_text(title, "LAYERTIME SETTINGS");

    // One scrolling list below the header.
    _page = lv_obj_create(_screen);
    lv_obj_set_size(_page, 240, 240 - kTop);
    lv_obj_set_pos(_page, 0, kTop);
    lv_obj_set_style_bg_opa(_page, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_page, 0, 0);
    lv_obj_set_style_pad_all(_page, 4, 0);
    lv_obj_set_style_pad_row(_page, 6, 0);
    lv_obj_set_flex_flow(_page, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(_page, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(_page, LV_SCROLLBAR_MODE_AUTO);

    lv_obj_t *dateTime = theme::button(_page, "DATE / TIME", theme::gold(), &lv_font_montserrat_14, 228, kRowHeight);
    lv_obj_add_event_cb(dateTime, dateTimeThunk, LV_EVENT_CLICKED, this);

    // BRIGHTNESS: a label and its value, then the slider, as on the Ultra.
    lv_obj_t *box = rowBox(_page, 18);
    lv_obj_t *label = lv_label_create(box);
    lv_obj_set_pos(label, 4, 2);
    lv_obj_set_style_text_color(label, theme::white(), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    lv_label_set_text(label, "BRIGHTNESS");
    _brightnessValue = valueLabel(box, theme::teal(), "--");
    lv_obj_set_y(_brightnessValue, 2);
    _brightnessSlider = lv_slider_create(_page);
    lv_obj_set_size(_brightnessSlider, 212, 14);
    lv_slider_set_range(_brightnessSlider, S3PlusLocalSettings::kMinBrightness, 255);
    lv_obj_set_style_bg_color(_brightnessSlider, theme::muted(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(_brightnessSlider, theme::teal(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(_brightnessSlider, theme::gold(), LV_PART_KNOB);
    lv_obj_add_event_cb(_brightnessSlider, brightnessThunk, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(_brightnessSlider, brightnessReleasedThunk, LV_EVENT_RELEASED, this);

    row("CLOCK FORMAT", clockFormatThunk, &_clockValue);
    row("UNITS", unitsThunk, &_unitsValue);
    row("GPS", gpsThunk, &_gpsValue);
    laterRow("MESHCORE");
    laterRow("MESHCORE ADVERTISE");
    laterRow("MESHTASTIC");
    laterRow("MESHTASTIC ADVERTISE");
    laterRow("MESHTASTIC NAME");
    row("EARLY WARNING", earlyWarningThunk, &_earlyWarningValue);
    laterRow("LOGGING");
    row("SLEEP MODE", sleepModeThunk, &_sleepModeValue);
    laterRow("SQUACHIFY?");
    row("DONGLE ICON", dongleIconThunk, &_dongleIconValue);
}

lv_obj_t *SettingsScreen::row(const char *title, lv_event_cb_t onClick, lv_obj_t **value)
{
    lv_obj_t *box = rowBox(_page, kRowHeight);
    lv_obj_t *b = theme::button(box, title, theme::teal(), rowFont(title), kButtonWidth, kRowHeight);
    lv_obj_set_pos(b, 0, 0);
    lv_obj_add_event_cb(b, onClick, LV_EVENT_CLICKED, this);
    *value = valueLabel(box, theme::teal(), "--");
    return b;
}

lv_obj_t *SettingsScreen::laterRow(const char *title)
{
    lv_obj_t *box = rowBox(_page, kRowHeight);
    lv_obj_t *b = theme::button(box, title, theme::muted(), rowFont(title), kButtonWidth, kRowHeight);
    lv_obj_set_pos(b, 0, 0);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICKABLE);
    valueLabel(box, theme::muted(), "LATER");
    return b;
}

void SettingsScreen::show()
{
    refreshValues();
    lv_obj_scroll_to_y(_page, 0, LV_ANIM_OFF);
    lv_screen_load(_screen);
}

void SettingsScreen::render()
{
    // Values change only through this screen; nothing to poll.
}

void SettingsScreen::refreshValues()
{
    if (!_app) return;
    lv_slider_set_value(_brightnessSlider, _app->brightness(), LV_ANIM_OFF);
    lv_label_set_text_fmt(_brightnessValue, "%u", static_cast<unsigned>(_app->brightness()));
    lv_label_set_text(_clockValue, _app->use24Hour() ? "24 H" : "12 H");
    lv_label_set_text(_unitsValue, _app->metricUnits() ? "METRIC" : "IMPERIAL");
    lv_label_set_text(_gpsValue, _app->gpsEnabled() ? "ON" : "OFF");
    lv_label_set_text(_earlyWarningValue, _app->earlyWarningEnabled() ? "ON" : "OFF");
    lv_label_set_text(_sleepModeValue, _app->sleepModeEnabled() ? "ON" : "OFF");
    lv_label_set_text(_dongleIconValue, _app->dongleIconShown() ? "ON" : "OFF");
}

namespace {
SettingsScreen *self(lv_event_t *event)
{
    return static_cast<SettingsScreen *>(lv_event_get_user_data(event));
}
} // namespace

void SettingsScreen::backThunk(lv_event_t *event)
{
    SettingsScreen *s = self(event);
    if (s && s->_app) s->_app->showHome();
}

void SettingsScreen::dateTimeThunk(lv_event_t *event)
{
    SettingsScreen *s = self(event);
    if (s && s->_app) s->_app->showTime();
}

void SettingsScreen::brightnessThunk(lv_event_t *event)
{
    // Applied live while dragging; saved once, on release.
    SettingsScreen *s = self(event);
    if (!s || !s->_app) return;
    const int32_t v = lv_slider_get_value(s->_brightnessSlider);
    s->_app->setBrightness(static_cast<uint8_t>(v), false);
    lv_label_set_text_fmt(s->_brightnessValue, "%u", static_cast<unsigned>(v));
}

void SettingsScreen::brightnessReleasedThunk(lv_event_t *event)
{
    SettingsScreen *s = self(event);
    if (!s || !s->_app) return;
    s->_app->setBrightness(static_cast<uint8_t>(lv_slider_get_value(s->_brightnessSlider)), true);
}

void SettingsScreen::clockFormatThunk(lv_event_t *event)
{
    SettingsScreen *s = self(event);
    if (!s || !s->_app) return;
    s->_app->setUse24Hour(!s->_app->use24Hour());
    s->refreshValues();
}

void SettingsScreen::unitsThunk(lv_event_t *event)
{
    SettingsScreen *s = self(event);
    if (!s || !s->_app) return;
    s->_app->setMetricUnits(!s->_app->metricUnits());
    s->refreshValues();
}

void SettingsScreen::gpsThunk(lv_event_t *event)
{
    SettingsScreen *s = self(event);
    if (!s || !s->_app) return;
    s->_app->setGpsEnabled(!s->_app->gpsEnabled());
    s->refreshValues();
}

void SettingsScreen::earlyWarningThunk(lv_event_t *event)
{
    SettingsScreen *s = self(event);
    if (!s || !s->_app) return;
    s->_app->setEarlyWarning(!s->_app->earlyWarningEnabled());
    s->refreshValues();
}

void SettingsScreen::sleepModeThunk(lv_event_t *event)
{
    SettingsScreen *s = self(event);
    if (!s || !s->_app) return;
    s->_app->setSleepMode(!s->_app->sleepModeEnabled());
    s->refreshValues();
}

void SettingsScreen::dongleIconThunk(lv_event_t *event)
{
    SettingsScreen *s = self(event);
    if (!s || !s->_app) return;
    s->_app->setDongleIconShown(!s->_app->dongleIconShown());
    s->refreshValues();
}

} // namespace twatch_s3plus
} // namespace layertime
