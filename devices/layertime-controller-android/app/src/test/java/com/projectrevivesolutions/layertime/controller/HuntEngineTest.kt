package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.hunt.HuntEngine
import com.projectrevivesolutions.layertime.controller.hunt.HuntPhase
import com.projectrevivesolutions.layertime.controller.hunt.SignalBand
import com.projectrevivesolutions.layertime.controller.hunt.Trend
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/** The hunt math (HuntEngine) with the default bands: green -65, yellow -85, FOUND -50 for 2 s, 2 s smoothing. */
class HuntEngineTest {
    private fun heard(e: HuntEngine, dbm: Int, at: Long, n: Int = 2) = e.onReading(n, dbm, dbm, dbm, true, false, at)
    private fun quiet(e: HuntEngine, at: Long, lost: Boolean = false) = e.onReading(0, null, null, null, true, lost, at)

    @Test fun startsThenSearchesUntilHeard() {
        val e = HuntEngine(); e.start(0)
        assertEquals(HuntPhase.STARTING, e.phase(100))
        quiet(e, 220)
        assertEquals(HuntPhase.SEARCHING, e.phase(220))
        assertEquals(SignalBand.NONE, e.band(220))
        heard(e, -90, 440)
        assertEquals(HuntPhase.TRACKING, e.phase(440))
        assertEquals(SignalBand.RED, e.band(440))
    }

    @Test fun bandsFollowTheSmoothedSignal() {
        val e = HuntEngine(); e.start(0)
        heard(e, -80, 100)
        assertEquals(SignalBand.YELLOW, e.band(100))
        var t = 100L
        repeat(10) { t += 220; heard(e, -60, t) } // the old -80 ages out of the 2 s window
        assertEquals(SignalBand.GREEN, e.band(t))
        assertEquals(-60, e.view(t).smoothedDbm)
    }

    @Test fun smoothingWeightsByReadings() {
        val e = HuntEngine(); e.start(0)
        e.onReading(3, -60, -60, -60, true, false, 100)
        e.onReading(1, -80, -80, -80, true, false, 300)
        assertEquals(-65, e.view(300).smoothedDbm) // (3 * -60 + -80) / 4
    }

    @Test fun lostAfterFiveSecondsOrWhenTheWandSaysSo() {
        val e = HuntEngine(); e.start(0)
        heard(e, -70, 100)
        quiet(e, 5000)
        assertEquals(HuntPhase.TRACKING, e.phase(5000))
        assertEquals(HuntPhase.LOST, e.phase(5100))
        assertNull(e.view(5100).smoothedDbm)
        val f = HuntEngine(); f.start(0)
        heard(f, -70, 100); quiet(f, 300, lost = true)
        assertEquals(HuntPhase.LOST, f.phase(300))
        heard(f, -70, 500)
        assertEquals(HuntPhase.TRACKING, f.phase(500))
    }

    @Test fun foundNeedsTwoSecondsAboveTheLine() {
        val e = HuntEngine(); e.start(0)
        var t = 0L
        repeat(8) { t += 220; heard(e, -45, t) } // 1.54 s above -50
        assertEquals(HuntPhase.TRACKING, e.phase(t))
        repeat(3) { t += 220; heard(e, -45, t) }
        assertEquals(HuntPhase.FOUND, e.phase(t))
        // FOUND stays until the next hunt.
        repeat(5) { t += 220; heard(e, -90, t) }
        assertEquals(HuntPhase.FOUND, e.phase(t))
        e.start(t)
        assertEquals(HuntPhase.STARTING, e.phase(t))
    }

    @Test fun aDipResetsTheFoundClock() {
        val e = HuntEngine(); e.start(0)
        var t = 0L
        repeat(7) { t += 220; heard(e, -45, t) }
        repeat(6) { t += 220; heard(e, -75, t) } // smoothed falls below -50
        repeat(7) { t += 220; heard(e, -45, t) }
        assertEquals(HuntPhase.TRACKING, e.phase(t))
    }

    @Test fun trendSaysClosingAndOpening() {
        val e = HuntEngine(); e.start(0)
        var t = 0L; var dbm = -90
        repeat(20) { t += 220; dbm += 1; heard(e, dbm, t) }
        assertEquals(Trend.CLOSING, e.view(t).trend)
        repeat(20) { t += 220; dbm -= 1; heard(e, dbm, t) }
        assertEquals(Trend.OPENING, e.view(t).trend)
        repeat(20) { t += 220; heard(e, dbm, t) }
        assertEquals(Trend.STEADY, e.view(t).trend)
    }

    @Test fun stepsColorTheTrail() {
        val e = HuntEngine(); e.start(0)
        e.onStep(10)
        heard(e, -80, 100); e.onStep(110)
        var t = 100L
        repeat(10) { t += 220; heard(e, -60, t) }
        e.onStep(t)
        val v = e.view(t)
        assertEquals(3, v.steps)
        assertEquals(listOf(SignalBand.NONE, SignalBand.YELLOW, SignalBand.GREEN), v.trail)
    }

    @Test fun ringsCloseInAsTheSignalRises() {
        val e = HuntEngine()
        assertNull(e.ringFor(null))
        assertEquals(10, e.ringFor(-100)) // weaker than the floor: the outer ring
        assertEquals(10, e.ringFor(-95))
        assertEquals(1, e.ringFor(-50))   // FOUND strength: the center ring
        assertEquals(1, e.ringFor(-40))
        var last = 11
        for (dbm in -95..-50) { val r = e.ringFor(dbm)!!; assertTrue(r <= last); last = r }
        // The view carries it, and loses it when the target is lost.
        e.start(0)
        e.onReading(2, -50, -50, -50, true, false, 100)
        assertEquals(1, e.view(100).ring)
        assertNull(e.view(5200).ring)
    }

    @Test fun toneSpeedsUpAndRisesWithTheSignal() {
        val e = HuntEngine(); e.start(0)
        assertNull(e.toneFor(e.view(0)))
        heard(e, -90, 100)
        val slow = e.toneFor(e.view(100))!!
        var t = 100L
        repeat(10) { t += 220; heard(e, -55, t) }
        val fast = e.toneFor(e.view(t))!!
        assertTrue(fast.intervalMs < slow.intervalMs)
        assertTrue(fast.freqHz > slow.freqHz)
        assertNotNull(fast)
    }
}
