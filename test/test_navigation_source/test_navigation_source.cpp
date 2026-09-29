// Unit tests for the T-Ultra NavigationSource adapter
// (src/platform/twatch_ultra/TUltraNavigationSource.h): WatchState's GNSS
// fields into the contract's NavigationState, in SI units, with every
// invalid quantity reported as 0 (contracts/models.md, NavigationState).

#include "check.h"

#include <cmath>
#include <initializer_list>

#include <Arduino.h>

#include "platform/twatch_ultra/TUltraNavigationSource.h"

using namespace layertime;
using layertime::twatch_ultra::TUltraNavigationSource;

namespace {

WatchState fullFix()
{
    WatchState s;
    s.gpsEnabled = true;
    s.gpsFix = true;
    s.gpsFixType = 3;
    s.gpsEverHadFix = true;
    s.gpsFixAgeMs = 850;
    s.gpsSatellites = 11;
    s.latitude = 47.6062;
    s.longitude = -122.3321;
    s.gpsAltitudeValid = true;
    s.altitudeFt = 328.0839895f;  // 100 m
    s.gpsHorizontalAccuracyValid = true;
    s.gpsHorizontalAccuracyM = 1.6f;
    s.gpsVerticalAccuracyValid = true;
    s.gpsVerticalAccuracyM = 2.4f;
    s.gpsHdopValid = true;
    s.gpsHdop = 1.49f;
    s.gpsSpeedValid = true;
    s.gpsSpeedMph = 10.0f;
    s.gpsCourseValid = true;
    s.gpsCourseDegrees = 271.5f;
    return s;
}

} // namespace

void a_full_fix_converts_to_si()
{
    NavigationState n;
    TUltraNavigationSource::convert(fullFix(), 12345, n);
    CHECK_TRUE(n.receiverEnabled);
    CHECK_INT(static_cast<int>(FixType::Fix3D), static_cast<int>(n.fixType));
    CHECK_TRUE(n.fixUsable);
    CHECK_TRUE(n.everHadFix);
    CHECK_INT(850, n.fixAgeMs);
    CHECK_INT(11, n.satellites);
    CHECK_NEAR(47.6062, n.latitudeDeg, 0.0);
    CHECK_NEAR(-122.3321, n.longitudeDeg, 0.0);
    CHECK_TRUE(n.altitudeValid);
    CHECK_NEAR(100.0, n.altitudeM, 1e-4);
    CHECK_TRUE(n.horizontalAccuracyValid);
    CHECK_NEAR(1.6, n.horizontalAccuracyM, 1e-6);
    CHECK_TRUE(n.verticalAccuracyValid);
    CHECK_NEAR(2.4, n.verticalAccuracyM, 1e-6);
    CHECK_TRUE(n.hdopValid);
    CHECK_NEAR(1.49, n.hdop, 1e-6);
    CHECK_TRUE(n.speedValid);
    CHECK_NEAR(4.4704, n.speedMps, 1e-5);
    CHECK_TRUE(n.courseValid);
    CHECK_NEAR(271.5, n.courseDeg, 0.0);
    CHECK_FALSE(n.headingValid);
    CHECK_INT(12345, n.updated.uptimeMs);
    CHECK_FALSE(n.updated.wallClockValid);
}

void altitude_uses_the_screens_feet_per_metre()
{
    // The watch face shows metric altitude as int(altitudeFt / 3.280839895f).
    // The adapter divides by the same float, so the same value comes out.
    WatchState s = fullFix();
    for (float ft : {0.0f, 1.0f, 3.28f, 3.29f, 1234.5f, 14505.0f, -282.0f}) {
        s.altitudeFt = ft;
        NavigationState n;
        TUltraNavigationSource::convert(s, 0, n);
        CHECK_TRUE(n.altitudeM == ft / 3.280839895f);
    }
}

void invalid_quantities_read_zero_even_if_stale_values_remain()
{
    WatchState s = fullFix();
    s.gpsAltitudeValid = false;
    s.gpsHorizontalAccuracyValid = false;
    s.gpsVerticalAccuracyValid = false;
    s.gpsHdopValid = false;
    s.gpsSpeedValid = false;
    s.gpsCourseValid = false;
    NavigationState n;
    n.altitudeM = 99.0f;  // anything from a previous read is overwritten
    TUltraNavigationSource::convert(s, 0, n);
    CHECK_FALSE(n.altitudeValid);
    CHECK_NEAR(0.0, n.altitudeM, 0.0);
    CHECK_FALSE(n.horizontalAccuracyValid);
    CHECK_NEAR(0.0, n.horizontalAccuracyM, 0.0);
    CHECK_FALSE(n.verticalAccuracyValid);
    CHECK_NEAR(0.0, n.verticalAccuracyM, 0.0);
    CHECK_FALSE(n.hdopValid);
    CHECK_NEAR(0.0, n.hdop, 0.0);
    CHECK_FALSE(n.speedValid);
    CHECK_NEAR(0.0, n.speedMps, 0.0);
    CHECK_FALSE(n.courseValid);
    CHECK_NEAR(0.0, n.courseDeg, 0.0);
}

void fix_types_follow_ubx_and_unknown_ones_read_none()
{
    WatchState s = fullFix();
    const FixType expected[] = {FixType::None, FixType::DeadReckoningOnly, FixType::Fix2D,
                                FixType::Fix3D, FixType::None, FixType::None, FixType::None};
    for (uint8_t v = 0; v < 7; ++v) {
        s.gpsFixType = v;
        NavigationState n;
        TUltraNavigationSource::convert(s, 0, n);
        CHECK_INT(static_cast<int>(expected[v]), static_cast<int>(n.fixType));
    }
}

void receiver_off_is_reported_as_is()
{
    WatchState s;  // GpsService's state with the receiver off
    NavigationState n;
    TUltraNavigationSource::convert(s, 7, n);
    CHECK_FALSE(n.receiverEnabled);
    CHECK_FALSE(n.fixUsable);
    CHECK_FALSE(n.everHadFix);
    CHECK_INT(static_cast<int>(FixType::None), static_cast<int>(n.fixType));
}

void read_stamps_the_platform_clock()
{
    WatchState s = fullFix();
    TUltraNavigationSource source(s);
    fake_arduino::g_millis = 777;
    NavigationState n;
    source.read(n);
    CHECK_INT(777, n.updated.uptimeMs);
    CHECK_TRUE(n.fixUsable);
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(a_full_fix_converts_to_si);
    CASE(altitude_uses_the_screens_feet_per_metre);
    CASE(invalid_quantities_read_zero_even_if_stale_values_remain);
    CASE(fix_types_follow_ubx_and_unknown_ones_read_none);
    CASE(receiver_off_is_reported_as_is);
    CASE(read_stamps_the_platform_clock);
    CHECK_SUMMARY();
}
