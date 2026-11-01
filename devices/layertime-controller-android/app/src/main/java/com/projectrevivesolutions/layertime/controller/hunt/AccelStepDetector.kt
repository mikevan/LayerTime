package com.projectrevivesolutions.layertime.controller.hunt

import kotlin.math.sqrt

/**
 * Steps from the accelerometer, for the hunt's step trail. Pure Kotlin; unit-tested
 * (AccelStepDetectorTest).
 *
 * Why: on the LOKMAT watch (2026-10-31) the step detector sensor is listed and the
 * Physical activity permission was granted, yet it never delivered a step, so the trail
 * stayed empty and STEPS read 0. The accelerometer always delivers.
 *
 * How: the length of the acceleration vector does not depend on how the wrist is turned.
 * A slow average tracks gravity; a fast average follows the motion. Each step is a bump of
 * the fast average above the slow one by more than [thresholdMs2]; the detector re-arms
 * once the bump falls back below [rearmMs2], and never counts two steps closer than
 * [minIntervalMs].
 */
class AccelStepDetector(
    private val thresholdMs2: Float = 1.2f,
    private val rearmMs2: Float = 0.3f,
    private val minIntervalMs: Long = 300,
) {
    private var started = false
    private var slow = 0f
    private var fast = 0f
    private var armed = true
    private var lastStepMs = Long.MIN_VALUE / 2

    /** One accelerometer sample in m/s². True when it completes a step. */
    fun onSample(x: Float, y: Float, z: Float, tMs: Long): Boolean {
        val mag = sqrt(x * x + y * y + z * z)
        if (!started) { started = true; slow = mag; fast = mag; return false }
        slow += SLOW * (mag - slow)
        fast += FAST * (mag - fast)
        val bump = fast - slow
        if (bump < rearmMs2) armed = true
        if (armed && bump > thresholdMs2 && tMs - lastStepMs >= minIntervalMs) {
            armed = false
            lastStepMs = tMs
            return true
        }
        return false
    }

    private companion object {
        const val SLOW = 0.02f
        const val FAST = 0.3f
    }
}
