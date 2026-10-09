// The words the S3 Plus screens show (devices/lilygo-s3plus/src/ui/TextFormat): the home
// screen's blocks, which read as the T-Ultra's watch face does, the Time
// screen's ages, and one Recon results entry.
//
// Run from devices/lilygo-s3plus/test/:
//   g++ -std=c++17 -O0 -Wall -Wextra -I../../../test -I../../../src -I../../../sensors/src -I../src -o tests_s3plus_text test_s3plus_text/test_s3plus_text.cpp ../src/ui/TextFormat.cpp ../../../src/core/logic/ReconSelection.cpp ../../../src/core/logic/GeoGrid.cpp ../src/gnss/Solar.cpp
//   ./tests_s3plus_text

#include "check.h"

#include <string.h>

#include "ui/TextFormat.h"

using namespace layertime;
using namespace layertime::twatch_s3plus;

namespace {
gnss::DateTime at(int y, int mo, int d, int h, int mi)
{
    gnss::DateTime t;
    t.year = y;
    t.month = mo;
    t.day = d;
    t.hour = h;
    t.minute = mi;
    return t;
}
}

void day_of_week_matches_the_calendar()
{
    CHECK_INT(4, text::dayOfWeek(2026, 10, 1)); // Thursday 1 October 2026
    CHECK_INT(3, text::dayOfWeek(2026, 9, 30));
    CHECK_INT(4, text::dayOfWeek(2026, 1, 1));
    CHECK_INT(1, text::dayOfWeek(2024, 2, 26));
    CHECK_INT(4, text::dayOfWeek(2024, 2, 29));
    CHECK_INT(6, text::dayOfWeek(2000, 1, 1));
}

void time_reads_like_the_ultras_face()
{
    char out[24];
    text::formatTime(at(2026, 10, 1, 14, 5), true, out, sizeof(out));
    CHECK_STR("14:05", out);
    text::formatTime(at(2026, 10, 1, 14, 5), false, out, sizeof(out));
    CHECK_STR("02:05", out);
    text::formatTime(at(2026, 10, 1, 0, 7), false, out, sizeof(out));
    CHECK_STR("12:07", out);
    text::formatTime(at(2026, 10, 1, 12, 0), false, out, sizeof(out));
    CHECK_STR("12:00", out);
    text::formatTime(at(2026, 10, 1, 0, 7), true, out, sizeof(out));
    CHECK_STR("00:07", out);
}

void date_reads_like_the_ultras_face()
{
    char out[24];
    text::formatDate(at(2026, 10, 1, 0, 0), out, sizeof(out));
    CHECK_STR("THU | OCT 01", out);
    text::formatDate(at(2028, 2, 29, 0, 0), out, sizeof(out));
    CHECK_STR("TUE | FEB 29", out);
    text::formatDate(at(2026, 0, 1, 0, 0), out, sizeof(out));
    CHECK_STR("--- | --- --", out);
}

void ages_read_long_on_the_time_screen_and_short_on_the_face()
{
    char out[16];
    text::formatAge(0, out, sizeof(out));
    CHECK_STR("0 s", out);
    text::formatAge(59999, out, sizeof(out));
    CHECK_STR("59 s", out);
    text::formatAge(60000, out, sizeof(out));
    CHECK_STR("1 min", out);
    text::formatAge(7200000, out, sizeof(out));
    CHECK_STR("2 h", out);
    text::formatShortAge(45000, out, sizeof(out));
    CHECK_STR("45S", out);
    text::formatShortAge(125000, out, sizeof(out));
    CHECK_STR("2M", out);
    text::formatShortAge(3600000, out, sizeof(out));
    CHECK_STR("1H", out);
}

void battery_block()
{
    char out[16];
    text::formatBattery(87, out, sizeof(out));
    CHECK_STR("BAT 87%", out);
    text::formatBattery(-1, out, sizeof(out));
    CHECK_STR("BAT --", out);
}

void gps_page_lines_read_like_the_ultras_gps_page()
{
    char out[48];
    NavigationState n;
    n.receiverEnabled = true;  // GNSS on, as S3PlusNavigationSource reports it
    n.satellites = 4;
    text::formatGpsStatus(n, out, sizeof(out));
    CHECK_STR("ACQUIRING", out);
    text::formatLatitude(n, out, sizeof(out));
    CHECK_STR("LAT  --", out);
    text::formatLongitude(n, out, sizeof(out));
    CHECK_STR("LON  --", out);
    text::formatMgrs(n, out, sizeof(out));
    CHECK_STR("WAITING FOR FIX", out);
    text::formatAltitudeLine(n, false, out, sizeof(out));
    CHECK_STR("ALT  -- FT", out);
    text::formatAccuracy(n, true, out, sizeof(out));
    CHECK_STR("ACC  -- M", out);
    text::formatSpeed(n, false, out, sizeof(out));
    CHECK_STR("SPD  -- MPH", out);
    text::formatCourse(n, out, sizeof(out));
    CHECK_STR("DIRECTION OF TRAVEL  --", out);
    text::formatSatellites(n, out, sizeof(out));
    CHECK_STR("SATS  4", out);

    n.everHadFix = true;
    n.fixUsable = true;
    n.fixType = FixType::Fix3D;
    n.latitudeDeg = 35.0;
    n.longitudeDeg = -94.0;
    n.altitudeValid = true;
    n.altitudeM = 398.6f;
    n.horizontalAccuracyValid = true;
    n.horizontalAccuracyM = 1.5f;
    n.speedValid = true;
    n.speedMps = 1.4f;
    n.courseValid = true;
    n.courseDeg = 44.6f;
    text::formatGpsStatus(n, out, sizeof(out));
    CHECK_STR("3D FIX", out);
    text::formatLatitude(n, out, sizeof(out));
    CHECK_STR("LAT  35.000000", out);
    text::formatLongitude(n, out, sizeof(out));
    CHECK_STR("LON  -94.000000", out);
    text::formatMgrs(n, out, sizeof(out));
    CHECK_TRUE(strncmp(out, "15S", 3) == 0); // zone 15, band S at 35N 94W
    CHECK_TRUE(strchr(out, '\n') == nullptr); // one line on this page

    // The position from the 0.2.2 photo, on one line (core writes it on two).
    {
        NavigationState here = n;
        here.latitudeDeg = 36.282187;
        here.longitudeDeg = -94.202290;
        text::formatMgrs(here, out, sizeof(out));
        CHECK_STR("15S UA 92025 15918", out);
    }
    text::formatAltitudeLine(n, false, out, sizeof(out));
    CHECK_STR("ALT  1307 FT", out);
    text::formatAltitudeLine(n, true, out, sizeof(out));
    CHECK_STR("ALT  398 M", out);
    text::formatAccuracy(n, true, out, sizeof(out));
    CHECK_STR("ACC  1.5 M", out);
    text::formatAccuracy(n, false, out, sizeof(out));
    CHECK_STR("ACC  4.9 FT", out);
    text::formatSpeed(n, false, out, sizeof(out));
    CHECK_STR("SPD  3.1 MPH", out);
    text::formatSpeed(n, true, out, sizeof(out));
    CHECK_STR("SPD  5.0 KM/H", out);
    text::formatCourse(n, out, sizeof(out));
    CHECK_STR("DIRECTION OF TRAVEL  045 DEG", out);

    // At rest: a usable fix still reports a speed (1.9 mph of noise, as 0.2.2
    // showed on a desk), but the receiver's accuracy estimates fail
    // courseIsMeaningful, so courseValid is false and no speed is shown.
    {
        NavigationState still = n;
        still.speedMps = 0.85f;
        still.courseValid = false;
        text::formatSpeed(still, false, out, sizeof(out));
        CHECK_STR("SPD  -- MPH", out);
        text::formatSpeed(still, true, out, sizeof(out));
        CHECK_STR("SPD  -- KM/H", out);
    }

    // A lost fix: position hidden, status says how long ago.
    n.fixUsable = false;
    n.fixAgeMs = 125000;
    text::formatGpsStatus(n, out, sizeof(out));
    CHECK_STR("FIX LOST 2M AGO", out);
    text::formatLatitude(n, out, sizeof(out));
    CHECK_STR("LAT  --", out);
    text::formatMgrs(n, out, sizeof(out));
    CHECK_STR("WAITING FOR FIX", out);
}

void a_results_entry_reads_like_the_ultras()
{
    char out[200];
    MonitorEvent e;
    e.detector = ReconTarget::Deauth;
    e.confidence = Confidence::High;
    strcpy(e.sourceId, "AA:BB:CC:DD:EE:FF");
    strcpy(e.detail, "Deauth burst");
    e.rssi = -67;
    e.channel = 6;
    e.count = 3;
    text::formatEvent(e, out, sizeof(out));
    CHECK_STR("DEAUTH  [HIGH]\nDeauth burst\nAA:BB:CC:DD:EE:FF  -67 dBm  CH 6  x3\n\n", out);
    e.detector = ReconTarget::AirTag;
    e.confidence = Confidence::Medium;
    e.channel = 0;
    e.count = 1;
    text::formatEvent(e, out, sizeof(out));
    CHECK_STR("AIRTAG  [MED]\nDeauth burst\nAA:BB:CC:DD:EE:FF  -67 dBm  x1\n\n", out);
}

void gps_switched_off_reads_off_as_on_the_ultra()
{
    char out[48];
    NavigationState n;  // receiverEnabled false: GPS switched off in Settings
    text::formatGpsStatus(n, out, sizeof(out));
    CHECK_STR("GPS OFF", out);
}

// ---- the watch face --------------------------------------------------------

void watch_temperature_decodes_the_bma423_register()
{
    int c = 0;
    CHECK_TRUE(text::watchCelsius(0x00, c));
    CHECK_INT(23, c);
    CHECK_TRUE(text::watchCelsius(0x0A, c));
    CHECK_INT(33, c);
    // Below 23 C: SensorLib reads 0xFF as 278 C; it is 22 C.
    CHECK_TRUE(text::watchCelsius(0xFF, c));
    CHECK_INT(22, c);
    CHECK_TRUE(text::watchCelsius(0xE2, c));  // -30
    CHECK_INT(-7, c);
    CHECK_FALSE(text::watchCelsius(0x80, c));  // no valid reading
}

void watch_temperature_reads_in_the_wearers_units()
{
    char out[16];
    text::formatWatchTemperature(true, 33, false, out, sizeof(out));
    CHECK_STR("91\xC2\xB0" "F", out);
    text::formatWatchTemperature(true, 33, true, out, sizeof(out));
    CHECK_STR("33\xC2\xB0" "C", out);
    text::formatWatchTemperature(true, -7, false, out, sizeof(out));
    CHECK_STR("19\xC2\xB0" "F", out);
    text::formatWatchTemperature(false, 0, false, out, sizeof(out));
    CHECK_STR("--", out);
    CHECK_FALSE(text::watchIsHot(44));
    CHECK_TRUE(text::watchIsHot(45));
}

void battery_dots_and_the_low_line()
{
    CHECK_INT(0, text::batteryDotsLit(0, 24));
    CHECK_INT(1, text::batteryDotsLit(1, 24));    // any charge shows a dot
    CHECK_INT(20, text::batteryDotsLit(82, 24));
    CHECK_INT(24, text::batteryDotsLit(100, 24));
    CHECK_INT(0, text::batteryDotsLit(-1, 24));   // unknown
    CHECK_TRUE(text::batteryIsLow(20));
    CHECK_FALSE(text::batteryIsLow(21));
    CHECK_FALSE(text::batteryIsLow(-1));
}

void recon_bracket_reads_like_the_garmin()
{
    char out[32];
    ReconState s;
    text::formatReconMode(s, out, sizeof(out));
    CHECK_STR("RECON OFF", out);
    s.earlyWarningEnabled = true;
    text::formatReconMode(s, out, sizeof(out));
    CHECK_STR("EARLY WARN", out);
    s.earlyWarningResting = true;
    text::formatReconMode(s, out, sizeof(out));
    CHECK_STR("EARLY WARN  REST", out);
    s.monitoring = true;
    s.selected = ReconTarget::All;
    text::formatReconMode(s, out, sizeof(out));
    CHECK_STR("RECON ALL", out);
    s.selected = ReconTarget::CounterIntrusion;
    text::formatReconMode(s, out, sizeof(out));
    CHECK_STR("RECON INTRUSION", out);
    text::formatDetections(0, out, sizeof(out));
    CHECK_STR("0 DETECTIONS", out);
    text::formatDetections(1, out, sizeof(out));
    CHECK_STR("1 DETECTION", out);
    text::formatDetections(40, out, sizeof(out));
    CHECK_STR("40 DETECTIONS", out);
}

void solar_ring_reads_the_next_event_on_the_faces_clock()
{
    char title[12], value[12];
    solar::NextEvent e;
    text::formatSolar(e, -300, false, title, sizeof(title), value, sizeof(value));
    CHECK_STR("SUN", title);
    CHECK_STR("--", value);
    gnss::DateTime utc;
    utc.year = 2026; utc.month = 10; utc.day = 2; utc.hour = 0; utc.minute = 0;
    e.known = true;
    e.isSunrise = false;
    e.utcMinutes = solar::minutesSinceEpoch(utc);  // 19:00 at UTC-5
    text::formatSolar(e, -300, false, title, sizeof(title), value, sizeof(value));
    CHECK_STR("SUNSET", title);
    CHECK_STR("07:00", value);
    text::formatSolar(e, -300, true, title, sizeof(title), value, sizeof(value));
    CHECK_STR("19:00", value);
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(day_of_week_matches_the_calendar);
    CASE(time_reads_like_the_ultras_face);
    CASE(date_reads_like_the_ultras_face);
    CASE(ages_read_long_on_the_time_screen_and_short_on_the_face);
    CASE(battery_block);
    CASE(gps_page_lines_read_like_the_ultras_gps_page);
    CASE(a_results_entry_reads_like_the_ultras);
    CASE(gps_switched_off_reads_off_as_on_the_ultra);
    CASE(watch_temperature_decodes_the_bma423_register);
    CASE(watch_temperature_reads_in_the_wearers_units);
    CASE(battery_dots_and_the_low_line);
    CASE(recon_bracket_reads_like_the_garmin);
    CASE(solar_ring_reads_the_next_event_on_the_faces_clock);
    CHECK_SUMMARY();
}
