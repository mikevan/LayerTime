// Characterization tests for the clock and altitude on src/ui/WatchFace:
// 12- or 24-hour time, and altitude in feet or metres. Added in Phase 0
// Step 6, when those two settings moved into the core, and run against the
// face both before and after that move with identical results.
//
// The face is compiled against the test-only LVGL fake, with the logo
// widgets replaced by no-op fakes. Everything goes through the Harness, so
// only it knows where the two settings live.

#include "check.h"

#include <string>

#include "core/app/LayerTimeCore.h"
#include "ui/WatchFace.h"

// ---- link fakes for the two logo widgets
void OwlLogo::create(lv_obj_t *, int, int, int, int) {}
void OwlLogo::setHidden(bool) {}
bool SquachLogo::create(lv_obj_t *, int, int, int, int) { return false; }
void SquachLogo::setHidden(bool) {}

namespace {

// The face as WatchApp draws it: T-Ultra settings, the core's application
// settings (changed by command, as the Settings rows do), and Recon state.
struct Harness {
    WatchFace face;
    WatchState state;
    AppSettings settings;
    layertime::LayerTimeCore core;

    Harness()
    {
        fake_lv::reset();
        lv_screen_load(lv_obj_create(nullptr));
        face.create();
        state.month = 9;
        state.day = 28;
    }
    ~Harness() { fake_lv::reset(); }

    void set(layertime::CommandType t, bool on)
    {
        layertime::LayerTimeCommand c;
        c.type = t;
        c.setting.enabled = on;
        core.execute(c);
    }
    void clock24(bool on) { set(layertime::CommandType::SetClockFormat, on); }
    void metric(bool on) { set(layertime::CommandType::SetUnits, on); }
    void render() { face.render(state, settings, core.settings(), core.reconState()); }
    void time(int hour, int minute)
    {
        state.hour = hour;
        state.minute = minute;
    }
    void altitude(bool valid, float feet)
    {
        state.gpsAltitudeValid = valid;
        state.altitudeFt = feet;
    }
    std::string clock()
    {
        render();
        for (lv_obj_t *l : fake_lv::visibleLabels())
            if (l->text.size() == 5 && l->text[2] == ':') return l->text;
        return "<none>";
    }
    std::string altitudeText()
    {
        render();
        lv_obj_t *l = fake_lv::findVisibleLabelStartingWith("ALT\n");
        return l ? l->text : std::string("<none>");
    }
};

} // namespace

void twelve_hour_clock_by_default()
{
    Harness h;
    h.time(0, 5);
    CHECK_STR("12:05", h.clock().c_str());
    h.time(12, 5);
    CHECK_STR("12:05", h.clock().c_str());
    h.time(13, 45);
    CHECK_STR("01:45", h.clock().c_str());
    h.time(23, 59);
    CHECK_STR("11:59", h.clock().c_str());
}

void twenty_four_hour_clock_when_set()
{
    Harness h;
    h.clock24(true);
    h.time(0, 5);
    CHECK_STR("00:05", h.clock().c_str());
    h.time(13, 45);
    CHECK_STR("13:45", h.clock().c_str());
    h.clock24(false);
    h.time(13, 45);
    CHECK_STR("01:45", h.clock().c_str());
}

void altitude_in_feet_by_default()
{
    Harness h;
    h.altitude(true, 1234.7f);
    CHECK_STR("ALT\n1234 FT", h.altitudeText().c_str());
    h.altitude(false, 1234.7f);
    CHECK_STR("ALT\n-- FT", h.altitudeText().c_str());
}

void altitude_in_metres_when_metric()
{
    Harness h;
    h.metric(true);
    h.altitude(true, 1234.7f);
    CHECK_STR("ALT\n376 M", h.altitudeText().c_str());
    h.altitude(true, -100.0f);
    CHECK_STR("ALT\n-30 M", h.altitudeText().c_str());
    h.altitude(false, 1234.7f);
    CHECK_STR("ALT\n-- M", h.altitudeText().c_str());
}

void clock_and_units_are_independent()
{
    Harness h;
    h.clock24(true);
    h.altitude(true, 1000.0f);
    CHECK_STR("ALT\n1000 FT", h.altitudeText().c_str());
    h.metric(true);
    h.time(13, 0);
    CHECK_STR("13:00", h.clock().c_str());
    CHECK_STR("ALT\n304 M", h.altitudeText().c_str());
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(twelve_hour_clock_by_default);
    CASE(twenty_four_hour_clock_when_set);
    CASE(altitude_in_feet_by_default);
    CASE(altitude_in_metres_when_metric);
    CASE(clock_and_units_are_independent);
    CHECK_SUMMARY();
}
