// Characterization tests for the declination instruction text built in
// src/ui/MappingScreen.cpp, as it stands.
//
// The screen is compiled unchanged against the test-only LVGL fake, together
// with the real DeclinationCalculator and GeoGrid, and driven through its
// public show/render API and its on-screen buttons. The tests read back the
// three labels the wearer acts on: the source line, the big offset value,
// and the advice sentence.
//
// Two kinds of check:
//   * Golden strings for fixed places and a fixed date, recorded from the
//     code as it stands on 2026-09-28. These fail if the numbers move.
//   * Rule checks that recompute the offset from DeclinationCalculator and
//     GeoGrid directly, so a failure says whether the TEXT or the MATH moved.
//
// Rounding note: on the watch LVGL's builtin printf formats these numbers;
// here the C library does. They can differ only on an exact half-way value,
// so every input is checked to be well clear of one before it is used.

#include "check.h"

#include <cmath>
#include <string>

#include <Arduino.h>

#include "core/logic/DeclinationCalculator.h"
#include "core/logic/GeoGrid.h"
#include "ui/MappingScreen.h"

namespace {

struct Place { double lat; double lon; };
const Place kSeattle{47.6062, -122.3321};
const Place kBangor{44.8016, -68.7712};
const Place kFortBragg{35.139, -79.006};

struct Harness {
    MappingScreen screen;
    WatchState state;
    AppSettings settings;

    Harness()
    {
        fake_lv::reset();
        screen.create(nullptr, nullptr);
        state.year = 2026;
        state.month = 9;
        state.day = 28;
    }
    ~Harness() { fake_lv::reset(); }

    void fix(const Place &p)
    {
        state.gpsFix = true;
        state.latitude = p.lat;
        state.longitude = p.lon;
    }

    void show() { screen.show(state, settings); }
    void render() { screen.render(state, settings); }
    void press(const char *label) { fake_lv::click(fake_lv::findVisibleLabel(label)); }

    std::string text(const char *prefix)
    {
        lv_obj_t *l = fake_lv::findVisibleLabelStartingWith(prefix);
        return l ? l->text : std::string("<missing>");
    }
    std::string source()
    {
        for (const char *s : {"FROM GPS FIX", "MANUAL LOCATION", "NO GPS FIX"})
            if (fake_lv::findVisibleLabel(s)) return s;
        return "<missing>";
    }
    std::string value()
    {
        for (lv_obj_t *l : fake_lv::visibleLabels())
            if (l->text.find(" DEG ") != std::string::npos || l->text == "NO LOCATION") return l->text;
        return "<missing>";
    }
    std::string advice()
    {
        for (const char *p : {"Compass reads", "Waiting for", "Set a location"}) {
            lv_obj_t *l = fake_lv::findVisibleLabelStartingWith(p);
            if (l) return l->text;
        }
        return "<missing>";
    }

    void enterManualLocation(const char *text)
    {
        press("ELSEWHERE");
        lv_textarea_set_text(fake_lv::findVisibleTextarea(), text);
        press("SAVE");
    }
};

double gridOffset(const Place &p)
{
    const double y = DeclinationCalculator::decimalYear(2026, 9, 28);
    return DeclinationCalculator::declinationDegrees(p.lat, p.lon, y) -
           GeoGrid::convergenceDegrees(p.lat, p.lon);
}

double trueOffset(const Place &p)
{
    const double y = DeclinationCalculator::decimalYear(2026, 9, 28);
    return DeclinationCalculator::declinationDegrees(p.lat, p.lon, y);
}

// True when a value is far enough from a one-decimal rounding boundary that
// any correct printf rounds it the same way.
bool clearOfHalfway(double v)
{
    const double tenths = std::fabs(v) * 10.0;
    return std::fabs((tenths - std::floor(tenths)) - 0.5) > 0.01;
}

// The two sentences as the screen builds them, for rule checks.
std::string expectedAdvice(double offset, const char *north)
{
    char buf[160];
    if (offset >= 0.0)
        snprintf(buf, sizeof(buf),
                 "Compass reads low. ADD %.1f deg to a compass bearing to get a %s bearing.",
                 offset, north);
    else
        snprintf(buf, sizeof(buf),
                 "Compass reads high. SUBTRACT %.1f deg from a compass bearing to get a %s bearing.",
                 -offset, north);
    return buf;
}

std::string expectedValue(double offset)
{
    char buf[32];
    if (offset >= 0.0) snprintf(buf, sizeof(buf), "%.1f DEG EAST", offset);
    else snprintf(buf, sizeof(buf), "%.1f DEG WEST", -offset);
    return buf;
}

} // namespace

void inputs_are_clear_of_rounding_boundaries()
{
    for (const Place &p : {kSeattle, kBangor, kFortBragg}) {
        CHECK_TRUE(clearOfHalfway(gridOffset(p)));
        CHECK_TRUE(clearOfHalfway(trueOffset(p)));
    }
}

void no_fix_asks_for_one()
{
    Harness h;
    h.show();
    CHECK_STR("NO GPS FIX", h.source().c_str());
    CHECK_STR("-- , --", h.text("--").c_str());
    CHECK_STR("NO LOCATION", h.value().c_str());
    CHECK_STR("Waiting for a GPS fix. Tap ELSEWHERE to enter a location instead.",
              h.advice().c_str());
}

void grid_north_is_the_default_golden_east()
{
    Harness h;
    h.fix(kSeattle);
    h.show();
    CHECK_STR("FROM GPS FIX", h.source().c_str());
    CHECK_STR("47.606200, -122.332100", h.text("47.").c_str());
    CHECK_STR("14.4 DEG EAST", h.value().c_str());
    CHECK_STR("Compass reads low. ADD 14.4 deg to a compass bearing to get a grid bearing.",
              h.advice().c_str());
}

void true_north_uses_plain_declination_golden_east()
{
    Harness h;
    h.fix(kSeattle);
    h.show();
    h.press("TRUE");
    CHECK_STR("14.9 DEG EAST", h.value().c_str());
    CHECK_STR("Compass reads low. ADD 14.9 deg to a compass bearing to get a true bearing.",
              h.advice().c_str());
    h.press("GRID");
    CHECK_STR("14.4 DEG EAST", h.value().c_str());
}

void west_offsets_say_subtract_golden_west()
{
    Harness h;
    h.fix(kBangor);
    h.show();
    CHECK_STR("15.3 DEG WEST", h.value().c_str());
    CHECK_STR("Compass reads high. SUBTRACT 15.3 deg from a compass bearing to get a grid bearing.",
              h.advice().c_str());
    h.press("TRUE");
    CHECK_STR("15.1 DEG WEST", h.value().c_str());
    CHECK_STR("Compass reads high. SUBTRACT 15.1 deg from a compass bearing to get a true bearing.",
              h.advice().c_str());
}

void text_follows_the_calculator_for_grid_and_true()
{
    for (const Place &p : {kSeattle, kBangor, kFortBragg}) {
        Harness h;
        h.fix(p);
        h.show();
        CHECK_STR(expectedValue(gridOffset(p)).c_str(), h.value().c_str());
        CHECK_STR(expectedAdvice(gridOffset(p), "grid").c_str(), h.advice().c_str());
        h.press("TRUE");
        CHECK_STR(expectedValue(trueOffset(p)).c_str(), h.value().c_str());
        CHECK_STR(expectedAdvice(trueOffset(p), "true").c_str(), h.advice().c_str());
    }
}

void manual_location_replaces_the_gps_fix()
{
    Harness h;
    h.fix(kSeattle);
    h.show();
    h.enterManualLocation("44.8016, -68.7712");
    CHECK_STR("MANUAL LOCATION", h.source().c_str());
    CHECK_STR("44.801600, -68.771200", h.text("44.").c_str());
    CHECK_STR("15.3 DEG WEST", h.value().c_str());
    h.press("HERE");
    CHECK_STR("FROM GPS FIX", h.source().c_str());
    CHECK_STR("14.4 DEG EAST", h.value().c_str());
}

void manual_location_works_without_a_fix()
{
    Harness h;
    h.show();
    h.enterManualLocation("35.139,-79.006");
    CHECK_STR("MANUAL LOCATION", h.source().c_str());
    CHECK_STR(expectedValue(gridOffset(kFortBragg)).c_str(), h.value().c_str());
}

void invalid_manual_location_keeps_the_entry_page_open()
{
    Harness h;
    h.fix(kSeattle);
    h.show();
    h.press("ELSEWHERE");
    lv_textarea_set_text(fake_lv::findVisibleTextarea(), "91, 10");
    h.press("SAVE");
    CHECK_STR("INVALID - USE LAT, LON (E.G. 47.6062, -122.3321)",
              h.text("INVALID").c_str());
    h.press("CANCEL");
    CHECK_STR("FROM GPS FIX", h.source().c_str());
    CHECK_STR("14.4 DEG EAST", h.value().c_str());
}

void render_refreshes_only_while_the_screen_is_showing()
{
    Harness h;
    h.show();  // no fix yet
    lv_obj_t *value = fake_lv::findVisibleLabel("NO LOCATION");
    CHECK_TRUE(value != nullptr);
    if (!value) return;

    h.fix(kSeattle);
    lv_screen_load(nullptr);  // another screen is up
    h.render();
    CHECK_STR("NO LOCATION", value->text.c_str());

    lv_screen_load(h.screen.screen());
    h.press("ELSEWHERE");  // location entry page open
    h.render();
    CHECK_STR("NO LOCATION", value->text.c_str());

    h.press("CANCEL");
    h.render();
    CHECK_STR("14.4 DEG EAST", value->text.c_str());
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(inputs_are_clear_of_rounding_boundaries);
    CASE(no_fix_asks_for_one);
    CASE(grid_north_is_the_default_golden_east);
    CASE(true_north_uses_plain_declination_golden_east);
    CASE(west_offsets_say_subtract_golden_west);
    CASE(text_follows_the_calculator_for_grid_and_true);
    CASE(manual_location_replaces_the_gps_fix);
    CASE(manual_location_works_without_a_fix);
    CASE(invalid_manual_location_keeps_the_entry_page_open);
    CASE(render_refreshes_only_while_the_screen_is_showing);
    CHECK_SUMMARY();
}
