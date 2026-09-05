#include "check.h"
#include "services/GeoGrid.h"

namespace {
constexpr double kMm = 0.001;

struct Fix { const char *name; double lat; double lon; int zone; char band;
             double easting; double northing; const char *mgrs; };

const Fix kFixtures[] = {
    {"rogers_ar", 36.2822536, -94.2021878, 15, 'S', 392034.6899, 4015925.5729, "15SUA9203415925"},
    {"zone15_central_merid", 36.28, -93.0, 15, 'S', 500000.0000, 4015005.2733, "15SWA0000015005"},
    {"zone14_side_of_seam", 36.28, -96.000001, 14, 'S', 769460.9648, 4019181.8407, "14SQF6946019181"},
    {"zone15_side_of_seam", 36.28, -96.0, 15, 'S', 230538.9454, 4019181.8435, "15STA3053819181"},
    {"equator_cm", 0.0, -93.0, 15, 'N', 500000.0000, 0.0000, "15NWA0000000000"},
    {"southern_cape_town", -33.9, 18.4, 34, 'H', 259583.2217, 6245888.0454, "34HBH5958345888"},
    {"southern_deep", -70.0, 18.4, 34, 'D', 400780.7995, 2232010.9687, "34DDH0078032010"},
    {"northern_high", 83.999999, -93.0, 15, 'X', 500000.0000, 9328093.7189, "15XWP0000028093"},
    {"arctic_canada", 75.0, -93.0, 15, 'X', 500000.0000, 8323606.8122, "15XWD0000023606"},
    {"arctic_alaska", 75.0, -150.0, 6, 'X', 413362.9617, 8325798.2470, "06XVJ1336225798"},
    {"greenland", 75.0, -45.0, 23, 'X', 500000.0000, 8323606.8122, "23XND0000023606"},
    {"svalbard_z33", 78.0, 20.9, 33, 'X', 636716.8460, 8665261.5498, "33XXG3671665261"},
    {"norway_z32", 60.0, 5.0, 32, 'V', 276979.9264, 6658157.2024, "32VKM7697958157"},
};

void squash(const char *in, char *out, size_t n) {
    size_t j = 0;
    for (const char *p = in; *p && j + 1 < n; ++p)
        if (*p != ' ' && *p != '\n' && *p != '\r') out[j++] = *p;
    out[j] = '\0';
}
}

void zone_from_plain_longitude() {
    CHECK_INT(1,  GeoGrid::utmZoneFor(0.0, -180.0));
    CHECK_INT(1,  GeoGrid::utmZoneFor(0.0, -175.0));
    CHECK_INT(15, GeoGrid::utmZoneFor(36.28, -93.0));
    CHECK_INT(31, GeoGrid::utmZoneFor(0.0, 0.0));
    CHECK_INT(60, GeoGrid::utmZoneFor(0.0, 179.999));
}
void zone_boundaries_exclusive_upper() {
    CHECK_INT(14, GeoGrid::utmZoneFor(36.28, -96.000001));
    CHECK_INT(15, GeoGrid::utmZoneFor(36.28, -96.0));
    CHECK_INT(15, GeoGrid::utmZoneFor(36.28, -90.000001));
    CHECK_INT(16, GeoGrid::utmZoneFor(36.28, -90.0));
}
void zone_clamps_to_valid_range() {
    CHECK_TRUE(GeoGrid::utmZoneFor(0.0, -181.0) >= 1);
    CHECK_TRUE(GeoGrid::utmZoneFor(0.0,  181.0) <= 60);
}
void zone_norway_applies() {
    CHECK_INT(32, GeoGrid::utmZoneFor(60.0, 5.0));
    CHECK_INT(32, GeoGrid::utmZoneFor(56.0, 3.0));
    CHECK_INT(32, GeoGrid::utmZoneFor(63.999, 11.999));
}
void zone_norway_respects_bounds() {
    CHECK_INT(31, GeoGrid::utmZoneFor(55.999, 5.0));
    CHECK_INT(31, GeoGrid::utmZoneFor(64.0, 5.0));
    CHECK_INT(31, GeoGrid::utmZoneFor(60.0, 2.999));
    CHECK_INT(33, GeoGrid::utmZoneFor(60.0, 12.0));
}
void zone_svalbard_applies() {
    CHECK_INT(31, GeoGrid::utmZoneFor(73.0, 0.0));
    CHECK_INT(31, GeoGrid::utmZoneFor(73.0, 8.9));
    CHECK_INT(33, GeoGrid::utmZoneFor(73.0, 9.0));
    CHECK_INT(33, GeoGrid::utmZoneFor(78.0, 20.9));
    CHECK_INT(35, GeoGrid::utmZoneFor(78.0, 21.0));
    CHECK_INT(35, GeoGrid::utmZoneFor(80.0, 32.9));
    CHECK_INT(37, GeoGrid::utmZoneFor(80.0, 33.0));
    CHECK_INT(37, GeoGrid::utmZoneFor(83.0, 41.9));
}
void zone_svalbard_respects_latitude_band() {
    CHECK_INT(31, GeoGrid::utmZoneFor(71.999, 3.0));
    CHECK_INT(31, GeoGrid::utmZoneFor(84.0, 5.0));
}
void zone_svalbard_respects_eastern_bound() {
    CHECK_INT(38, GeoGrid::utmZoneFor(83.0, 42.0));
    CHECK_INT(39, GeoGrid::utmZoneFor(83.0, 48.0));
}
// THE REGRESSION TEST for the arctic bug.
void zone_arctic_west_of_prime_meridian_is_not_svalbard() {
    CHECK_INT(6,  GeoGrid::utmZoneFor(75.0, -150.0));
    CHECK_INT(15, GeoGrid::utmZoneFor(75.0, -93.0));
    CHECK_INT(23, GeoGrid::utmZoneFor(75.0, -45.0));
    CHECK_INT(30, GeoGrid::utmZoneFor(75.0, -1.0));
    CHECK_INT(15, GeoGrid::utmZoneFor(72.0, -93.0));
    CHECK_INT(15, GeoGrid::utmZoneFor(83.999999, -93.0));
}
void band_letters() {
    CHECK_CHAR('C', GeoGrid::latitudeBand(-80.0));
    CHECK_CHAR('N', GeoGrid::latitudeBand(0.0));
    CHECK_CHAR('S', GeoGrid::latitudeBand(36.28));
    CHECK_CHAR('X', GeoGrid::latitudeBand(75.0));
}
void band_omits_i_and_o() {
    for (double lat = -80.0; lat <= 83.0; lat += 1.0) {
        const char b = GeoGrid::latitudeBand(lat);
        CHECK_TRUE(b != 'I'); CHECK_TRUE(b != 'O');
    }
}
void band_rejects_out_of_range() {
    CHECK_CHAR('-', GeoGrid::latitudeBand(-80.000001));
    CHECK_CHAR('-', GeoGrid::latitudeBand(84.000001));
    CHECK_CHAR('-', GeoGrid::latitudeBand(90.0));
    CHECK_CHAR('-', GeoGrid::latitudeBand(-90.0));
}
void central_meridian() {
    CHECK_NEAR(-177.0, GeoGrid::centralMeridian(1), 1e-9);
    CHECK_NEAR(-93.0,  GeoGrid::centralMeridian(15), 1e-9);
    CHECK_NEAR(3.0,    GeoGrid::centralMeridian(31), 1e-9);
    CHECK_NEAR(177.0,  GeoGrid::centralMeridian(60), 1e-9);
}
void convergence_zero_on_central_meridian() {
    CHECK_NEAR(0.0, GeoGrid::convergenceDegrees(36.28, -93.0), 1e-9);
    // 50N 3E is below the Norway band, so this really is zone 31, CM 3E.
    CHECK_NEAR(0.0, GeoGrid::convergenceDegrees(50.0, 3.0), 1e-9);
}

// The Norway exception widens zone 32 to cover 3E-12E, double the usual six
// degrees. A position at its western edge is therefore SIX degrees off the
// central meridian, not three, and convergence reaches about 5.2 degrees.
// This is correct behaviour and it contradicts the "roughly 3 degrees"
// comment in GeoGrid.h, which is only true for normal-width zones.
void convergence_is_larger_inside_the_widened_norway_zone() {
    CHECK_INT(32, GeoGrid::utmZoneFor(60.0, 3.0));
    CHECK_NEAR(9.0, GeoGrid::centralMeridian(32), 1e-9);
    CHECK_NEAR(-5.2009, GeoGrid::convergenceDegrees(60.0, 3.0), 0.001);
}
void convergence_sign() {
    CHECK_TRUE(GeoGrid::convergenceDegrees(36.28, -91.0) > 0.0);
    CHECK_TRUE(GeoGrid::convergenceDegrees(36.28, -95.0) < 0.0);
    CHECK_TRUE(GeoGrid::convergenceDegrees(-36.28, -91.0) < 0.0);
}
void convergence_magnitude_sane() {
    const double c = GeoGrid::convergenceDegrees(36.28, -90.000001);
    CHECK_TRUE(c < 3.0 && c > 1.0);
}
void toutm_matches_proj() {
    for (const Fix &f : kFixtures) {
        const GeoGrid::UtmCoordinate u = GeoGrid::toUtm(f.lat, f.lon);
        CHECK_TRUE(u.valid);
        CHECK_INT(f.zone, u.zone);
        CHECK_CHAR(f.band, u.band);
        CHECK_NEAR(f.easting,  u.easting,  kMm);
        CHECK_NEAR(f.northing, u.northing, kMm);
    }
}
void toutm_southern_false_northing() {
    const GeoGrid::UtmCoordinate u = GeoGrid::toUtm(-33.9, 18.4);
    CHECK_TRUE(u.valid);
    CHECK_TRUE(u.northing > 0.0);
    CHECK_TRUE(u.northing < 10000000.0);
    CHECK_TRUE(GeoGrid::toUtm(-0.00001, -93.0).northing > 9999000.0);
}
void toutm_rejects_out_of_range() {
    CHECK_FALSE(GeoGrid::toUtm(84.000001, -93.0).valid);
    CHECK_FALSE(GeoGrid::toUtm(-80.000001, -93.0).valid);
    CHECK_FALSE(GeoGrid::toUtm(90.0, 0.0).valid);
    CHECK_FALSE(GeoGrid::toUtm(36.28, 180.000001).valid);
    CHECK_FALSE(GeoGrid::toUtm(36.28, -180.000001).valid);
}
void toutm_accepts_exact_limits() {
    CHECK_TRUE(GeoGrid::toUtm(84.0, -93.0).valid);
    CHECK_TRUE(GeoGrid::toUtm(-80.0, -93.0).valid);
}
void tomgrs_matches_reference() {
    for (const Fix &f : kFixtures) {
        char raw[64] = {0};
        char got[64] = {0};
        const GeoGrid::UtmCoordinate u = GeoGrid::toUtm(f.lat, f.lon);
        if (!GeoGrid::toMgrs(u, raw, sizeof(raw))) {
            CHECK_TRUE(false);   // refused to format; do not compare a stale buffer
            continue;
        }
        squash(raw, got, sizeof(got));
        CHECK_STR(f.mgrs, got);
    }
}
void tomgrs_rejects_invalid() {
    char out[64];
    GeoGrid::UtmCoordinate bad;
    CHECK_FALSE(GeoGrid::toMgrs(bad, out, sizeof(out)));
    CHECK_FALSE(GeoGrid::toMgrs(GeoGrid::toUtm(36.28, -93.0), nullptr, 64));
}
void tomgrs_columns_differ_across_zone_seam() {
    char a[64], b[64], sa[64], sb[64];
    const GeoGrid::UtmCoordinate z14 = GeoGrid::toUtm(36.28, -96.000001);
    const GeoGrid::UtmCoordinate z15 = GeoGrid::toUtm(36.28, -96.0);
    CHECK_TRUE(GeoGrid::toMgrs(z14, a, sizeof(a)));
    CHECK_TRUE(GeoGrid::toMgrs(z15, b, sizeof(b)));
    squash(a, sa, sizeof(sa)); squash(b, sb, sizeof(sb));
    CHECK_INT(14, z14.zone); CHECK_INT(15, z15.zone);
    CHECK_TRUE(std::strncmp(sa + 3, sb + 3, 2) != 0);
}
void tomgrs_rows_alternate_by_zone_parity() {
    char a[64], b[64], sa[64], sb[64];
    const GeoGrid::UtmCoordinate odd  = GeoGrid::toUtm(36.28, -93.0);
    const GeoGrid::UtmCoordinate even = GeoGrid::toUtm(36.28, -87.0);
    CHECK_TRUE(GeoGrid::toMgrs(odd, a, sizeof(a)));
    CHECK_TRUE(GeoGrid::toMgrs(even, b, sizeof(b)));
    squash(a, sa, sizeof(sa)); squash(b, sb, sizeof(sb));
    CHECK_INT(1, odd.zone % 2); CHECK_INT(0, even.zone % 2);
    CHECK_TRUE(sa[4] != sb[4]);
}

int main(int argc, char **argv) {
    CHECK_MAIN(argc, argv);

    CASE(zone_from_plain_longitude);
    CASE(zone_boundaries_exclusive_upper);
    CASE(zone_clamps_to_valid_range);
    CASE(zone_norway_applies);
    CASE(zone_norway_respects_bounds);
    CASE(zone_svalbard_applies);
    CASE(zone_svalbard_respects_latitude_band);
    CASE(zone_svalbard_respects_eastern_bound);
    CASE(zone_arctic_west_of_prime_meridian_is_not_svalbard);
    CASE(band_letters);
    CASE(band_omits_i_and_o);
    CASE(band_rejects_out_of_range);
    CASE(central_meridian);
    CASE(convergence_zero_on_central_meridian);
    CASE(convergence_is_larger_inside_the_widened_norway_zone);
    CASE(convergence_sign);
    CASE(convergence_magnitude_sane);
    CASE(toutm_matches_proj);
    CASE(toutm_southern_false_northing);
    CASE(toutm_rejects_out_of_range);
    CASE(toutm_accepts_exact_limits);
    CASE(tomgrs_matches_reference);
    CASE(tomgrs_rejects_invalid);
    CASE(tomgrs_columns_differ_across_zone_seam);
    CASE(tomgrs_rows_alternate_by_zone_parity);
    CHECK_SUMMARY();
}
