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

#include "HomeScreen.h"

#include "../app/S3PlusApp.h"
#include "S3Theme.h"
#include "TextFormat.h"

namespace layertime {
namespace twatch_s3plus {

namespace {
// Layout, 240 x 240 (face draft 4 with the rings and icons swapped, approved
// 2026-10-01).
constexpr int32_t kDotY = 3;
constexpr int32_t kDotSize = 5;
constexpr int32_t kDotPitch = 9;
constexpr int32_t kDotX = 10;
constexpr int32_t kOwlX = 64;
constexpr int32_t kOwlY = 20;
constexpr int32_t kOwlSize = 112;
constexpr int32_t kRingSize = 58;
constexpr int32_t kLeftRingX = 4;
constexpr int32_t kRightRingX = 178;
constexpr int32_t kButtonsY = 177;
// The GPS and DONGLE icons sit beside the owl's ears, like a status bar under
// the battery: centred on y 53.
constexpr int32_t kIconHeight = 30;
constexpr int32_t kIconCentreY = 53;
constexpr int32_t kIconY = kIconCentreY - kIconHeight / 2;
// The rings flank the time, as the Garmin's TEMP and SUNSET rings do: centred
// halfway between the icons' band (82) and the buttons' top.
constexpr int32_t kRingY = (82 + kButtonsY) / 2 - kRingSize / 2;
// Montserrat 28 clears the rings ("22:47" touched them at 32).
constexpr int32_t kTimeY = 131;
constexpr int32_t kDateY = 162;
// The Garmin's bracket, scaled from its 454 px design: pointed ends 8 px
// deep, top and bottom edges 27 px in from each end.
constexpr int32_t kBracketTop = 203;
constexpr int32_t kBracketBottom = 238;
constexpr int32_t kBracketLeft = 4;
constexpr int32_t kBracketRight = 236;
constexpr int32_t kBracketPoint = 8;
constexpr int32_t kBracketEdge = 27;

lv_color_t dim() { return lv_color_hex(0x223030); }

lv_obj_t *label(lv_obj_t *parent, int32_t x, int32_t y, int32_t width, const lv_font_t *font,
                lv_color_t colour, const char *text)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_width(l, width);
    lv_obj_set_pos(l, x, y);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(l, colour, 0);
    lv_obj_set_style_text_font(l, font, 0);
    lv_label_set_text(l, text);
    return l;
}

lv_obj_t *bareBox(lv_obj_t *parent)
{
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICKABLE);
    return b;
}
} // namespace

void HomeScreen::create(S3PlusApp *app)
{
    _app = app;
    _screen = theme::screen();
    // A long press anywhere on the face opens Settings, as on the Ultra.
    lv_obj_add_flag(_screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_screen, longPressThunk, LV_EVENT_LONG_PRESSED, this);
    // A double-tap on the face sleeps the display. SHORT_CLICKED is not sent
    // after a long press, so the two gestures never collide. Nothing on the
    // face scrolls, and a scrollable screen can cancel a tap as a drag (the
    // T-Ultra lost its double-tap to that), so scrolling is off.
    lv_obj_add_event_cb(_screen, backgroundTapThunk, LV_EVENT_SHORT_CLICKED, this);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < kBatteryDots; ++i) {
        lv_obj_t *d = bareBox(_screen);
        lv_obj_set_size(d, kDotSize, kDotSize);
        lv_obj_set_pos(d, kDotX + i * kDotPitch, kDotY);
        lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(d, dim(), 0);
        _dots[i] = d;
    }
    _battery = label(_screen, 0, 10, 240, &lv_font_montserrat_10, theme::gold(), "BAT --");

    _owl.create(_screen, kOwlX, kOwlY, kOwlSize, kOwlSize);
    _temperature = ring(kLeftRingX, kRingY, theme::gold(), "WATCH");
    _solar = ring(kRightRingX, kRingY, theme::teal(), "SUN");
    _gps = icon(kLeftRingX + kRingSize / 2, kIconY, LV_SYMBOL_GPS, "GPS");
    _dongle = icon(kRightRingX + kRingSize / 2, kIconY, LV_SYMBOL_USB, "DONGLE");
    // The GPS icon opens the GPS page, as the GPS block did.
    lv_obj_add_flag(_gps.box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_gps.box, gpsThunk, LV_EVENT_CLICKED, this);

    _time = label(_screen, 0, kTimeY, 240, &lv_font_montserrat_28, theme::gold(), "--:--");
    _date = label(_screen, 0, kDateY, 240, &lv_font_montserrat_12, theme::teal(), "--- | --- --");

    lv_obj_t *location = theme::button(_screen, "LOCATION", theme::green(), &lv_font_montserrat_12, 112, 22);
    lv_obj_set_pos(location, 4, kButtonsY);
    lv_obj_add_event_cb(location, gpsThunk, LV_EVENT_CLICKED, this);
    lv_obj_t *recon = theme::button(_screen, "RECON", theme::teal(), &lv_font_montserrat_12, 112, 22);
    lv_obj_set_pos(recon, 124, kButtonsY);
    lv_obj_add_event_cb(recon, reconThunk, LV_EVENT_CLICKED, this);

    buildBracket();
}

HomeScreen::Ring HomeScreen::ring(int32_t x, int32_t y, lv_color_t colour, const char *title)
{
    Ring r;
    r.frame = bareBox(_screen);
    lv_obj_set_size(r.frame, kRingSize, kRingSize);
    lv_obj_set_pos(r.frame, x, y);
    lv_obj_set_style_radius(r.frame, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(r.frame, 3, 0);
    lv_obj_set_style_border_color(r.frame, colour, 0);
    r.title = label(r.frame, 3, 13, kRingSize - 6, &lv_font_montserrat_10, colour, title);
    r.value = label(r.frame, 3, 26, kRingSize - 6, &lv_font_montserrat_14, theme::white(), "--");
    return r;
}

HomeScreen::Icon HomeScreen::icon(int32_t centreX, int32_t y, const char *glyph, const char *text)
{
    Icon i;
    i.box = bareBox(_screen);
    lv_obj_set_size(i.box, 60, kIconHeight);
    lv_obj_set_pos(i.box, centreX - 30, y);
    i.glyph = label(i.box, 0, 0, 60, &lv_font_montserrat_16, theme::alert(), glyph);
    i.label = label(i.box, 0, 18, 60, &lv_font_montserrat_10, theme::alert(), text);
    return i;
}

void HomeScreen::setIcon(const Icon &i, bool connected)
{
    const lv_color_t c = connected ? theme::green() : theme::alert();
    lv_obj_set_style_text_color(i.glyph, c, 0);
    lv_obj_set_style_text_color(i.label, c, 0);
}

void HomeScreen::buildBracket()
{
    // A clear box over the bracket takes the tap; the lines and text inside
    // are not clickable.
    lv_obj_t *hit = bareBox(_screen);
    lv_obj_set_size(hit, kBracketRight - kBracketLeft, kBracketBottom - kBracketTop);
    lv_obj_set_pos(hit, kBracketLeft, kBracketTop);
    lv_obj_add_flag(hit, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(hit, threatsThunk, LV_EVENT_CLICKED, this);

    const int32_t t = kBracketTop, b = kBracketBottom, m = (t + b) / 2;
    const int32_t l = kBracketLeft, r = kBracketRight, k = kBracketPoint, e = kBracketEdge;
    const int32_t segs[8][4] = {{l + k, t, l, m}, {l, m, l + k, b}, {r - k, t, r, m}, {r, m, r - k, b},
                                {l + k, t, l + e, t}, {r - e, t, r - k, t}, {l + k, b, l + e, b},
                                {r - e, b, r - k, b}};
    for (int i = 0; i < 8; ++i) {
        _bracketPoints[i][0] = {static_cast<lv_value_precise_t>(segs[i][0]), static_cast<lv_value_precise_t>(segs[i][1])};
        _bracketPoints[i][1] = {static_cast<lv_value_precise_t>(segs[i][2]), static_cast<lv_value_precise_t>(segs[i][3])};
        lv_obj_t *line = lv_line_create(_screen);
        lv_line_set_points(line, _bracketPoints[i], 2);
        lv_obj_set_style_line_width(line, 2, 0);
        lv_obj_set_style_line_rounded(line, true, 0);
        lv_obj_set_style_line_color(line, theme::gold(), 0);
        lv_obj_remove_flag(line, LV_OBJ_FLAG_CLICKABLE);
        _bracketLines[i] = line;
    }
    _mode = label(_screen, 20, kBracketTop + 3, 200, &lv_font_montserrat_10, theme::teal(), "RECON OFF");
    _count = label(_screen, 20, kBracketTop + 15, 200, &lv_font_montserrat_16, theme::white(), "0 DETECTIONS");
}

void HomeScreen::show()
{
    _doubleTap.reset();
    lv_screen_load(_screen);
}

void HomeScreen::render(uint32_t nowMs)
{
    (void)nowMs;
    if (!_app || lv_screen_active() != _screen) return;
    char buf[48];
    char title[16];

    // Battery.
    const int percent = _app->batteryPercent();
    const bool low = text::batteryIsLow(percent);
    const int lit = text::batteryDotsLit(percent, kBatteryDots);
    const lv_color_t on = low ? theme::alert() : theme::gold();
    for (int i = 0; i < kBatteryDots; ++i) lv_obj_set_style_bg_color(_dots[i], i < lit ? on : dim(), 0);
    text::formatBattery(percent, buf, sizeof(buf));
    lv_label_set_text(_battery, buf);
    lv_obj_set_style_text_color(_battery, on, 0);

    // WATCH: the watch's own temperature; red when it runs hot.
    const bool tempValid = _app->watchTemperatureValid();
    text::formatWatchTemperature(tempValid, _app->watchCelsius(), _app->metricUnits(), buf, sizeof(buf));
    lv_label_set_text(_temperature.value, buf);
    const bool hot = tempValid && text::watchIsHot(_app->watchCelsius());
    const lv_color_t tempColour = hot ? theme::alert() : theme::gold();
    lv_obj_set_style_border_color(_temperature.frame, tempColour, 0);
    lv_obj_set_style_text_color(_temperature.title, tempColour, 0);

    // The next sunrise or sunset.
    text::formatSolar(_app->nextSolarEvent(), _app->timeZoneMinutes(), _app->use24Hour(), title,
                      sizeof(title), buf, sizeof(buf));
    lv_label_set_text(_solar.title, title);
    lv_label_set_text(_solar.value, buf);

    // GPS: green with a usable fix. DONGLE: no link on this watch yet.
    setIcon(_gps, _app->navigation().fixUsable);
    setIcon(_dongle, _app->dongleConnected());
    if (_app->dongleIconShown()) lv_obj_remove_flag(_dongle.box, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(_dongle.box, LV_OBJ_FLAG_HIDDEN);

    // Time and date.
    const gnss::DateTime &t = _app->localTime();
    text::formatTime(t, _app->use24Hour(), buf, sizeof(buf));
    lv_label_set_text(_time, buf);
    if (_app->clockTrusted()) {
        text::formatDate(t, buf, sizeof(buf));
        lv_label_set_text(_date, buf);
        lv_obj_set_style_text_color(_date, theme::teal(), 0);
    } else {
        // Neither GNSS nor the DATE / TIME page has set the clock this boot.
        lv_label_set_text(_date, "TIME NOT SET YET");
        lv_obj_set_style_text_color(_date, theme::muted(), 0);
    }

    // The Recon bracket, in the Garmin's wording and colours.
    const ReconSnapshot &r = _app->recon();
    text::formatReconMode(r.state, buf, sizeof(buf));
    lv_label_set_text(_mode, buf);
    text::formatDetections(r.count, buf, sizeof(buf));
    lv_label_set_text(_count, buf);
    const bool alert = r.state.alertPending;
    lv_color_t modeColour = theme::muted();
    if (alert || r.state.monitoring) modeColour = theme::alert();
    else if (r.state.earlyWarningEnabled) modeColour = theme::teal();
    lv_obj_set_style_text_color(_mode, modeColour, 0);
    for (lv_obj_t *line : _bracketLines)
        lv_obj_set_style_line_color(line, alert ? theme::alert() : theme::gold(), 0);
}

void HomeScreen::reconThunk(lv_event_t *event)
{
    auto *self = static_cast<HomeScreen *>(lv_event_get_user_data(event));
    if (self && self->_app) self->_app->showRecon();
}

void HomeScreen::gpsThunk(lv_event_t *event)
{
    auto *self = static_cast<HomeScreen *>(lv_event_get_user_data(event));
    if (self && self->_app) self->_app->showGps();
}

void HomeScreen::longPressThunk(lv_event_t *event)
{
    // Only a press on the face itself, not one that started on a button or
    // the bracket (those bubble no LONG_PRESSED here, but be explicit).
    if (lv_event_get_target(event) != lv_event_get_current_target(event)) return;
    auto *self = static_cast<HomeScreen *>(lv_event_get_user_data(event));
    if (self && self->_app) self->_app->showSettings();
}

void HomeScreen::backgroundTapThunk(lv_event_t *event)
{
    // Only a tap on the face itself. Buttons, the GPS icon, and the bracket
    // take their own taps; the owl, the rings, the labels, and the DONGLE
    // icon are not clickable, so taps on them land here.
    if (lv_event_get_target(event) != lv_event_get_current_target(event)) return;
    auto *self = static_cast<HomeScreen *>(lv_event_get_user_data(event));
    if (self && self->_app && self->_doubleTap.tap(lv_tick_get())) self->_app->sleepDisplay();
}

void HomeScreen::threatsThunk(lv_event_t *event)
{
    // The running scan's monitor, or ALL (ReconScreen::showMonitor decides).
    auto *self = static_cast<HomeScreen *>(lv_event_get_user_data(event));
    if (self && self->_app) self->_app->showReconMonitor(ReconTarget::All);
}

} // namespace twatch_s3plus
} // namespace layertime
