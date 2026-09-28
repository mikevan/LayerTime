// LayerTime - counter-intrusion and resilient-communications firmware
// for the LilyGo T-Watch Ultra.
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

#include "DeclinationCalculator.h"

#include <math.h>
#include <string.h>

namespace DeclinationCalculator {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kRadToDeg = 180.0 / kPi;

constexpr int kMaxOrder = 12;
constexpr int kSize = kMaxOrder + 1;   // 13
constexpr double kEpoch = 2025.0;      // WMM2025, valid 2025.0-2030.0

struct WmmRow {
    int n, m;
    double gnm, hnm, dgnm, dhnm;
};

// NOAA World Magnetic Model 2025 coefficients (WMM.COF, release 11/13/2024,
// valid 2025.0-2030.0). Verbatim from NOAA's published coefficient table.
constexpr WmmRow kWmmRows[] = {
    {1,0,-29351.8,0.0,12.0,0.0},
    {1,1,-1410.8,4545.4,9.7,-21.5},
    {2,0,-2556.6,0.0,-11.6,0.0},
    {2,1,2951.1,-3133.6,-5.2,-27.7},
    {2,2,1649.3,-815.1,-8.0,-12.1},
    {3,0,1361.0,0.0,-1.3,0.0},
    {3,1,-2404.1,-56.6,-4.2,4.0},
    {3,2,1243.8,237.5,0.4,-0.3},
    {3,3,453.6,-549.5,-15.6,-4.1},
    {4,0,895.0,0.0,-1.6,0.0},
    {4,1,799.5,278.6,-2.4,-1.1},
    {4,2,55.7,-133.9,-6.0,4.1},
    {4,3,-281.1,212.0,5.6,1.6},
    {4,4,12.1,-375.6,-7.0,-4.4},
    {5,0,-233.2,0.0,0.6,0.0},
    {5,1,368.9,45.4,1.4,-0.5},
    {5,2,187.2,220.2,0.0,2.2},
    {5,3,-138.7,-122.9,0.6,0.4},
    {5,4,-142.0,43.0,2.2,1.7},
    {5,5,20.9,106.1,0.9,1.9},
    {6,0,64.4,0.0,-0.2,0.0},
    {6,1,63.8,-18.4,-0.4,0.3},
    {6,2,76.9,16.8,0.9,-1.6},
    {6,3,-115.7,48.8,1.2,-0.4},
    {6,4,-40.9,-59.8,-0.9,0.9},
    {6,5,14.9,10.9,0.3,0.7},
    {6,6,-60.7,72.7,0.9,0.9},
    {7,0,79.5,0.0,-0.0,0.0},
    {7,1,-77.0,-48.9,-0.1,0.6},
    {7,2,-8.8,-14.4,-0.1,0.5},
    {7,3,59.3,-1.0,0.5,-0.8},
    {7,4,15.8,23.4,-0.1,0.0},
    {7,5,2.5,-7.4,-0.8,-1.0},
    {7,6,-11.1,-25.1,-0.8,0.6},
    {7,7,14.2,-2.3,0.8,-0.2},
    {8,0,23.2,0.0,-0.1,0.0},
    {8,1,10.8,7.1,0.2,-0.2},
    {8,2,-17.5,-12.6,0.0,0.5},
    {8,3,2.0,11.4,0.5,-0.4},
    {8,4,-21.7,-9.7,-0.1,0.4},
    {8,5,16.9,12.7,0.3,-0.5},
    {8,6,15.0,0.7,0.2,-0.6},
    {8,7,-16.8,-5.2,-0.0,0.3},
    {8,8,0.9,3.9,0.2,0.2},
    {9,0,4.6,0.0,-0.0,0.0},
    {9,1,7.8,-24.8,-0.1,-0.3},
    {9,2,3.0,12.2,0.1,0.3},
    {9,3,-0.2,8.3,0.3,-0.3},
    {9,4,-2.5,-3.3,-0.3,0.3},
    {9,5,-13.1,-5.2,0.0,0.2},
    {9,6,2.4,7.2,0.3,-0.1},
    {9,7,8.6,-0.6,-0.1,-0.2},
    {9,8,-8.7,0.8,0.1,0.4},
    {9,9,-12.9,10.0,-0.1,0.1},
    {10,0,-1.3,0.0,0.1,0.0},
    {10,1,-6.4,3.3,0.0,0.0},
    {10,2,0.2,0.0,0.1,-0.0},
    {10,3,2.0,2.4,0.1,-0.2},
    {10,4,-1.0,5.3,-0.0,0.1},
    {10,5,-0.6,-9.1,-0.3,-0.1},
    {10,6,-0.9,0.4,0.0,0.1},
    {10,7,1.5,-4.2,-0.1,0.0},
    {10,8,0.9,-3.8,-0.1,-0.1},
    {10,9,-2.7,0.9,-0.0,0.2},
    {10,10,-3.9,-9.1,-0.0,-0.0},
    {11,0,2.9,0.0,0.0,0.0},
    {11,1,-1.5,0.0,-0.0,-0.0},
    {11,2,-2.5,2.9,0.0,0.1},
    {11,3,2.4,-0.6,0.0,-0.0},
    {11,4,-0.6,0.2,0.0,0.1},
    {11,5,-0.1,0.5,-0.1,-0.0},
    {11,6,-0.6,-0.3,0.0,-0.0},
    {11,7,-0.1,-1.2,-0.0,0.1},
    {11,8,1.1,-1.7,-0.1,-0.0},
    {11,9,-1.0,-2.9,-0.1,0.0},
    {11,10,-0.2,-1.8,-0.1,0.0},
    {11,11,2.6,-2.3,-0.1,0.0},
    {12,0,-2.0,0.0,0.0,0.0},
    {12,1,-0.2,-1.3,0.0,-0.0},
    {12,2,0.3,0.7,-0.0,0.0},
    {12,3,1.2,1.0,-0.0,-0.1},
    {12,4,-1.3,-1.4,-0.0,0.1},
    {12,5,0.6,-0.0,-0.0,-0.0},
    {12,6,0.6,0.6,0.1,-0.0},
    {12,7,0.5,-0.1,-0.0,-0.0},
    {12,8,-0.1,0.8,0.0,0.0},
    {12,9,-0.4,0.1,0.0,-0.0},
    {12,10,-0.2,-1.0,-0.1,-0.0},
    {12,11,-1.3,0.1,-0.0,0.0},
    {12,12,-0.7,0.2,-0.1,-0.1},
};
constexpr size_t kRowCount = sizeof(kWmmRows) / sizeof(kWmmRows[0]);

// Normalized coefficient cache, built once on first use.
double g_c[kSize][kSize];
double g_cd[kSize][kSize];
double g_k[kSize][kSize];
double g_snorm[kSize * kSize];
double g_fn[kSize];
double g_fm[kSize];
bool g_initialized = false;

void init()
{
    memset(g_c, 0, sizeof(g_c));
    memset(g_cd, 0, sizeof(g_cd));
    memset(g_k, 0, sizeof(g_k));
    memset(g_snorm, 0, sizeof(g_snorm));

    for (size_t i = 0; i < kRowCount; ++i) {
        const WmmRow &r = kWmmRows[i];
        g_c[r.m][r.n] = r.gnm;
        g_cd[r.m][r.n] = r.dgnm;
        if (r.m != 0) {
            g_c[r.n][r.m - 1] = r.hnm;
            g_cd[r.n][r.m - 1] = r.dhnm;
        }
    }

    // Convert Schmidt semi-normalized Gauss coefficients into the
    // recursion-friendly form used by the summation below (standard WMM
    // reference algorithm - the same steps NOAA's own C implementation and
    // every faithful port, e.g. pygeomag, perform).
    g_snorm[0] = 1.0;
    g_fm[0] = 0.0;
    for (int n = 1; n <= kMaxOrder; ++n) {
        g_snorm[n] = g_snorm[n - 1] * static_cast<double>(2 * n - 1) / static_cast<double>(n);
        int j = 2;
        for (int m = 0; m <= n; ++m) {
            g_k[m][n] = static_cast<double>(((n - 1) * (n - 1)) - (m * m)) /
                        static_cast<double>((2 * n - 1) * (2 * n - 3));
            if (m > 0) {
                const double flnmj = static_cast<double>((n - m + 1) * j) / static_cast<double>(n + m);
                g_snorm[n + m * kSize] = g_snorm[n + (m - 1) * kSize] * sqrt(flnmj);
                j = 1;
                g_c[n][m - 1] = g_snorm[n + m * kSize] * g_c[n][m - 1];
                g_cd[n][m - 1] = g_snorm[n + m * kSize] * g_cd[n][m - 1];
            }
            g_c[m][n] = g_snorm[n + m * kSize] * g_c[m][n];
            g_cd[m][n] = g_snorm[n + m * kSize] * g_cd[m][n];
        }
        g_fn[n] = static_cast<double>(n + 1);
        g_fm[n] = static_cast<double>(n);
    }
    g_k[1][1] = 0.0;
    g_initialized = true;
}

}  // namespace

double decimalYear(int year, int month, int day)
{
    static const int kCumulativeDays[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    if (month < 1) month = 1;
    if (month > 12) month = 12;
    const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    const int daysInYear = leap ? 366 : 365;
    int dayOfYear = kCumulativeDays[month - 1] + day;
    if (leap && month > 2) dayOfYear += 1;
    return static_cast<double>(year) + static_cast<double>(dayOfYear - 1) / static_cast<double>(daysInYear);
}

double declinationDegrees(double latitudeDeg, double longitudeDeg, double time)
{
    if (!g_initialized) init();

    double tc[kSize][kSize];
    double dp[kSize][kSize];
    double sp[kSize] = {0};
    double cp[kSize] = {0};
    double pp[kSize] = {0};
    double p[kSize * kSize];
    memset(tc, 0, sizeof(tc));
    memset(dp, 0, sizeof(dp));
    memcpy(p, g_snorm, sizeof(p));

    sp[0] = 0.0;
    cp[0] = pp[0] = 1.0;
    dp[0][0] = 0.0;

    // WGS84 ellipsoid + WMM reference radius, all in km (altitude assumed 0 -
    // i.e. the WGS84 ellipsoid surface - a hiker's actual elevation changes
    // this by a negligible fraction of a degree).
    constexpr double a = 6378.137, b = 6356.7523142, re = 6371.2;
    constexpr double a2 = a * a, b2 = b * b, c2 = a2 - b2, a4 = a2 * a2, b4 = b2 * b2, c4 = a4 - b4;

    const double dt = time - kEpoch;

    const double rlon = longitudeDeg * kDegToRad;
    const double rlat = latitudeDeg * kDegToRad;
    const double srlon = sin(rlon), crlon = cos(rlon);
    const double srlat = sin(rlat), crlat = cos(rlat);
    const double srlat2 = srlat * srlat, crlat2 = crlat * crlat;
    sp[1] = srlon;
    cp[1] = crlon;

    const double q = sqrt(a2 - c2 * srlat2);
    const double q1 = 0.0;  // altitude (km) above the ellipsoid - see note above.
    const double q2 = ((q1 + a2) / (q1 + b2)) * ((q1 + a2) / (q1 + b2));
    const double ct = srlat / sqrt(q2 * crlat2 + srlat2);
    const double st = sqrt(1.0 - ct * ct);
    const double r2 = 2.0 * q1 + (a4 - c4 * srlat2) / (q * q);
    const double r = sqrt(r2);
    const double d = sqrt(a2 * crlat2 + b2 * srlat2);
    const double ca = (q1 + d) / r;
    const double sa = c2 * crlat * srlat / (r * d);

    for (int m = 2; m <= kMaxOrder; ++m) {
        sp[m] = sp[1] * cp[m - 1] + cp[1] * sp[m - 1];
        cp[m] = cp[1] * cp[m - 1] - sp[1] * sp[m - 1];
    }

    double aor = re / r;
    double ar = aor * aor;
    double br = 0.0, bt = 0.0, bp = 0.0, bpp = 0.0;

    for (int n = 1; n <= kMaxOrder; ++n) {
        ar = ar * aor;
        for (int m = 0; m <= n; ++m) {
            if (n == m) {
                p[n + m * kSize] = st * p[n - 1 + (m - 1) * kSize];
                dp[m][n] = st * dp[m - 1][n - 1] + ct * p[n - 1 + (m - 1) * kSize];
            } else if (n == 1 && m == 0) {
                p[n + m * kSize] = ct * p[n - 1 + m * kSize];
                dp[m][n] = ct * dp[m][n - 1] - st * p[n - 1 + m * kSize];
            } else if (n > 1 && n != m) {
                if (m > n - 2) p[n - 2 + m * kSize] = 0.0;
                if (m > n - 2) dp[m][n - 2] = 0.0;
                p[n + m * kSize] = ct * p[n - 1 + m * kSize] - g_k[m][n] * p[n - 2 + m * kSize];
                dp[m][n] = ct * dp[m][n - 1] - st * p[n - 1 + m * kSize] - g_k[m][n] * dp[m][n - 2];
            }

            tc[m][n] = g_c[m][n] + dt * g_cd[m][n];
            if (m != 0) tc[n][m - 1] = g_c[n][m - 1] + dt * g_cd[n][m - 1];

            const double par = ar * p[n + m * kSize];
            double temp1, temp2;
            if (m == 0) {
                temp1 = tc[m][n] * cp[m];
                temp2 = tc[m][n] * sp[m];
            } else {
                temp1 = tc[m][n] * cp[m] + tc[n][m - 1] * sp[m];
                temp2 = tc[m][n] * sp[m] - tc[n][m - 1] * cp[m];
            }
            bt = bt - ar * temp1 * dp[m][n];
            bp += g_fm[m] * temp2 * par;
            br += g_fn[n] * temp1 * par;

            if (st == 0.0 && m == 1) {
                if (n == 1) pp[n] = pp[n - 1];
                else pp[n] = ct * pp[n - 1] - g_k[m][n] * pp[n - 2];
                const double parp = ar * pp[n];
                bpp += g_fm[m] * temp2 * parp;
            }
        }
    }

    if (st == 0.0) bp = bpp;
    else bp /= st;

    const double bx = -bt * ca - br * sa;
    const double by = bp;

    return atan2(by, bx) * kRadToDeg;
}

}  // namespace DeclinationCalculator
