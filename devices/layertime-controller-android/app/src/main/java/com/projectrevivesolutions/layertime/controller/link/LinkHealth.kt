package com.projectrevivesolutions.layertime.controller.link

/**
 * Link health from the Status heartbeat (contracts/link.md: Status is notified once a
 * second, and "the heartbeat is what the interface watches for link health"). Pure, so
 * it is unit-tested on the JVM.
 *
 * A LayerWand that loses power or goes out of range does not always produce a GATT
 * disconnect promptly; on the LOKMAT watch the link stayed ONLINE after the LayerWand
 * was unplugged. So the interface times the link out itself: STALE after
 * [STALE_MS] of silence (the Garmin face's threshold), LOST after [LOST_MS], and on
 * LOST it drops the connection and reconnects. During a scheduled Recon window
 * (schedule 1) the Node may be silent until its next Report window, so the allowance
 * grows by nextReportS.
 */
object LinkHealth {
    const val STALE_MS = 3_000L
    const val LOST_MS = 8_000L

    enum class State { OK, STALE, LOST }

    fun allowanceMs(schedule: Int, nextReportS: Int): Long =
        if (schedule == 1) LOST_MS + nextReportS * 1_000L else LOST_MS

    fun state(silenceMs: Long, schedule: Int, nextReportS: Int): State = when {
        silenceMs >= allowanceMs(schedule, nextReportS) -> State.LOST
        silenceMs >= STALE_MS && schedule != 1 -> State.STALE
        else -> State.OK
    }
}
