package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.hunt.AccelStepDetector
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import kotlin.math.PI
import kotlin.math.sin

/** The hunt's accelerometer step counter, on synthetic samples at 50 Hz. */
class AccelStepDetectorTest {
    private val g = 9.81f

    /** Counts steps over [seconds] of samples whose vector length is g plus [wave] at time t (s). */
    private fun count(seconds: Int, tilt: Boolean = false, wave: (Double) -> Double): Int {
        val d = AccelStepDetector()
        var steps = 0
        for (i in 0 until seconds * 50) {
            val t = i / 50.0
            val m = (g + wave(t)).toFloat()
            // The same length split across axes when tilted: orientation must not matter.
            val hit = if (tilt) d.onSample(m * 0.6f, m * 0.8f, 0f, (t * 1000).toLong())
                      else d.onSample(0f, 0f, m, (t * 1000).toLong())
            if (hit) steps++
        }
        return steps
    }

    @Test fun walkingAtTwoStepsASecondCountsTwentyInTenSeconds() {
        val steps = count(10) { t -> 3.0 * sin(2 * PI * 2.0 * t) }
        assertTrue("steps $steps", steps in 18..21)
    }

    @Test fun theWayTheWristIsTurnedDoesNotMatter() {
        val steps = count(10, tilt = true) { t -> 3.0 * sin(2 * PI * 2.0 * t) }
        assertTrue("steps $steps", steps in 18..21)
    }

    @Test fun aStillWristCountsNothing() {
        val r = java.util.Random(7)
        assertEquals(0, count(10) { _ -> (r.nextDouble() - 0.5) * 0.4 })
    }

    @Test fun aFastShakeIsCappedAtTheMinimumInterval() {
        // 10 bumps a second would be 100 in 10 s; the 300 ms floor allows at most 34.
        val steps = count(10) { t -> 4.0 * sin(2 * PI * 10.0 * t) }
        assertTrue("steps $steps", steps <= 34)
    }
}
