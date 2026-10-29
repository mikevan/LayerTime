package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.ui.SunTimes
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.util.Calendar
import java.util.TimeZone

class SunTimesTest {
    private fun hm(millis: Long, tz: TimeZone): Int {
        val c = Calendar.getInstance(tz).apply { timeInMillis = millis }
        return c.get(Calendar.HOUR_OF_DAY) * 60 + c.get(Calendar.MINUTE)
    }

    // Washington, DC, 2026-10-29 (EDT): published sunrise about 7:28, sunset about 18:06.
    @Test fun washingtonDcLateOctober() {
        val tz = TimeZone.getTimeZone("America/New_York")
        val rise = SunTimes.event(38.8951, -77.0364, 2026, 10, 29, tz, true)!!
        val set = SunTimes.event(38.8951, -77.0364, 2026, 10, 29, tz, false)!!
        assertEquals(7 * 60 + 28.0, hm(rise, tz).toDouble(), 6.0)
        assertEquals(18 * 60 + 6.0, hm(set, tz).toDouble(), 6.0)
    }

    // Quito, near the equator, at the March equinox: about 6:15 and 18:20 local (UTC-5).
    @Test fun equatorEquinox() {
        val tz = TimeZone.getTimeZone("America/Guayaquil")
        val rise = SunTimes.event(-0.1807, -78.4678, 2026, 3, 20, tz, true)!!
        val set = SunTimes.event(-0.1807, -78.4678, 2026, 3, 20, tz, false)!!
        assertEquals(6 * 60 + 15.0, hm(rise, tz).toDouble(), 8.0)
        assertEquals(18 * 60 + 20.0, hm(set, tz).toDouble(), 8.0)
    }

    @Test fun polarNightHasNoSunrise() {
        assertNull(SunTimes.event(78.2232, 15.6267, 2026, 12, 21, TimeZone.getTimeZone("Europe/Oslo"), true))
    }

    @Test fun nextEventIsAfterNow() {
        val tz = TimeZone.getTimeZone("America/New_York")
        val noon = Calendar.getInstance(tz).apply { clear(); set(2026, 9, 29, 12, 0) }.timeInMillis
        val (isRise, at) = SunTimes.next(38.8951, -77.0364, noon, tz)!!
        assertTrue(!isRise && at > noon)
        val night = Calendar.getInstance(tz).apply { clear(); set(2026, 9, 29, 22, 0) }.timeInMillis
        val (isRise2, at2) = SunTimes.next(38.8951, -77.0364, night, tz)!!
        assertTrue(isRise2 && at2 > night)
    }
}
