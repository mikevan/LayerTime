// LayerTime - passive early-warning system. Connect IQ Device App for the
// Garmin tactix 8 AMOLED.
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

import Toybox.Lang;

// Round-trip statistics for the PING burst. Pure, so it is unit tested and
// the same arithmetic can be checked on the host. Rank statistics use the
// nearest-rank method on the sorted list (p95 = the value at rank
// ceil(0.95 n)); the median of an even count is the mean of the two middle
// values. Values are integer milliseconds; mean and median are rounded to
// the nearest millisecond.
module LinkBurstStats {

    function compute(rtts as Array<Number>) as Dictionary {
        var n = rtts.size();
        if (n == 0) {
            return {:count => 0, :min => 0, :median => 0, :mean => 0, :p95 => 0, :max => 0};
        }
        var sorted = rtts.slice(0, n) as Array<Number>;
        sorted.sort(null);
        var sum = 0l;
        for (var i = 0; i < n; i++) { sum += sorted[i]; }
        var median = n % 2 == 1 ? sorted[n / 2] : ((sorted[n / 2 - 1] + sorted[n / 2] + 1) / 2);
        return {
            :count => n,
            :min => sorted[0],
            :median => median,
            :mean => ((sum + n / 2) / n).toNumber(),
            :p95 => sorted[rank(95, n)],
            :max => sorted[n - 1]
        };
    }

    // Zero-based index of the nearest-rank percentile: ceil(p/100 * n) - 1.
    function rank(percent as Number, n as Number) as Number {
        var r = (percent * n + 99) / 100; // ceil without floats
        if (r < 1) { r = 1; }
        if (r > n) { r = n; }
        return r - 1;
    }
}
