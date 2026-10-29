package com.projectrevivesolutions.layertime.controller.ui

import java.util.Calendar
import java.util.TimeZone
import kotlin.math.acos
import kotlin.math.asin
import kotlin.math.atan
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.sin
import kotlin.math.tan

/**
 * Sunrise and sunset from latitude, longitude, and date, computed on the device
 * with no network. The algorithm is the U.S. Naval Observatory "Almanac for
 * Computers" (1990) method at the standard 90.833 degree zenith, accurate to
 * about two minutes at mid latitudes. Pure, so it is unit-tested on the JVM.
 */
object SunTimes {
    private const val ZENITH = 90.833

    private fun deg(r: Double) = Math.toDegrees(r)
    private fun rad(d: Double) = Math.toRadians(d)
    private fun norm(v: Double, m: Double) = ((v % m) + m) % m

    /**
     * Epoch millis of the event on the given local calendar date, or null when the sun
     * does not rise (or set) that day at that latitude.
     */
    fun event(lat: Double, lon: Double, year: Int, month: Int, day: Int, tz: TimeZone, rising: Boolean): Long? {
        val cal = Calendar.getInstance(TimeZone.getTimeZone("UTC")).apply {
            clear(); set(year, month - 1, day)
        }
        val n = cal.get(Calendar.DAY_OF_YEAR)
        val lngHour = lon / 15.0
        val t = n + ((if (rising) 6.0 else 18.0) - lngHour) / 24.0
        val m = 0.9856 * t - 3.289
        val l = norm(m + 1.916 * sin(rad(m)) + 0.020 * sin(rad(2 * m)) + 282.634, 360.0)
        var ra = norm(deg(atan(0.91764 * tan(rad(l)))), 360.0)
        ra += floor(l / 90.0) * 90.0 - floor(ra / 90.0) * 90.0
        ra /= 15.0
        val sinDec = 0.39782 * sin(rad(l))
        val cosDec = cos(asin(sinDec))
        val cosH = (cos(rad(ZENITH)) - sinDec * sin(rad(lat))) / (cosDec * cos(rad(lat)))
        if (cosH > 1.0 || cosH < -1.0) return null
        val h = (if (rising) 360.0 - deg(acos(cosH)) else deg(acos(cosH))) / 15.0
        val localMean = h + ra - 0.06571 * t - 6.622
        val ut = norm(localMean - lngHour, 24.0)
        var millis = cal.timeInMillis + (ut * 3_600_000.0).toLong()
        // The UTC hour can land on the neighbouring local date; pull it back onto the asked date.
        val local = Calendar.getInstance(tz)
        local.timeInMillis = millis
        val want = Calendar.getInstance(tz).apply { clear(); set(year, month - 1, day) }
        if (local.get(Calendar.DAY_OF_YEAR) != want.get(Calendar.DAY_OF_YEAR) || local.get(Calendar.YEAR) != want.get(Calendar.YEAR)) {
            millis += if (local.before(want)) 86_400_000L else -86_400_000L
        }
        return millis
    }

    /** The next sunrise or sunset after [nowMillis]: (true for sunrise / false for sunset, epoch millis). */
    fun next(lat: Double, lon: Double, nowMillis: Long, tz: TimeZone): Pair<Boolean, Long>? {
        val c = Calendar.getInstance(tz).apply { timeInMillis = nowMillis }
        for (offset in 0..1) {
            val d = (c.clone() as Calendar).apply { add(Calendar.DAY_OF_YEAR, offset) }
            val y = d.get(Calendar.YEAR); val mo = d.get(Calendar.MONTH) + 1; val da = d.get(Calendar.DAY_OF_MONTH)
            val rise = event(lat, lon, y, mo, da, tz, true)
            val set = event(lat, lon, y, mo, da, tz, false)
            listOfNotNull(rise?.let { true to it }, set?.let { false to it })
                .sortedBy { it.second }
                .firstOrNull { it.second > nowMillis }
                ?.let { return it }
        }
        return null
    }
}
