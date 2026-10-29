package com.projectrevivesolutions.layertime.controller.wireshark

/**
 * Wording of the diagnostic screen's stability verdict. Pure, unit-tested.
 *
 * The native-heap figure moves by a few KB over tens of thousands of
 * iterations from allocator bookkeeping alone (build 15 and 16 on the TCL:
 * +1 to +7 KB over 20,000 decodes). The threshold below is a DIAGNOSTIC
 * threshold that keeps that noise from being reported as growth. It is not a
 * leak test and it does not prove the absence of a leak. The raw figures are
 * always printed next to the verdict.
 */
object StabilityVerdict {
    /** Growth is reported only when BOTH limits are exceeded. */
    const val NATIVE_THRESHOLD_KB = 64L
    const val NATIVE_THRESHOLD_BYTES_PER_ITERATION = 1.0

    /** An RSS fall larger than this between two samples is reported as an observation. */
    const val RSS_DROP_REPORT_KB = 20_000L

    fun bytesPerIteration(nativeDeltaKb: Long, iterations: Int): Double =
        if (iterations <= 0) 0.0 else nativeDeltaKb * 1024.0 / iterations

    fun nativeGrewAboveThreshold(nativeDeltaKb: Long, iterations: Int): Boolean =
        nativeDeltaKb > NATIVE_THRESHOLD_KB &&
            bytesPerIteration(nativeDeltaKb, iterations) > NATIVE_THRESHOLD_BYTES_PER_ITERATION

    /**
     * Least-squares slope of native-heap-allocated over the second half of the
     * samples, in bytes per iteration. A single allocator step early in a run
     * (the LOKMAT watch: +300 KB between 6,000 and 8,000, then flat) makes the
     * start-to-end delta look like growth; the trend over the second half
     * separates a one-off step from a steady climb. Null with fewer than three
     * samples in the second half.
     */
    data class Trend(val bytesPerIteration: Double?, val samples: Int, val fromIteration: Int, val toIteration: Int) {
        fun describe(): String = if (bytesPerIteration == null) "not enough samples ($samples)"
            else "%+.2f bytes per iteration over %d samples (iterations %,d to %,d)".format(bytesPerIteration, samples, fromIteration, toIteration)
        val steadyGrowth: Boolean get() = bytesPerIteration != null && bytesPerIteration > NATIVE_THRESHOLD_BYTES_PER_ITERATION
    }

    fun secondHalfTrend(samples: List<Pair<Int, Long>>): Trend {
        val half = samples.drop(samples.size / 2)
        if (half.size < 3) return Trend(null, half.size, half.firstOrNull()?.first ?: 0, half.lastOrNull()?.first ?: 0)
        val n = half.size
        val mx = half.sumOf { it.first.toDouble() } / n
        val my = half.sumOf { it.second * 1024.0 } / n
        var sxy = 0.0; var sxx = 0.0
        for ((x, y) in half) { sxy += (x - mx) * (y * 1024.0 - my); sxx += (x - mx) * (x - mx) }
        val slope = if (sxx == 0.0) 0.0 else sxy / sxx
        return Trend(slope, n, half.first().first, half.last().first)
    }

    fun verdict(control: Boolean, mismatches: Int, nativeDeltaKb: Long, iterations: Int, rssDrops: Int,
                trend: Trend? = null): String {
        val perIter = bytesPerIteration(nativeDeltaKb, iterations)
        val main = when {
            !control && mismatches > 0 -> "OUTPUT MISMATCHES ($mismatches of $iterations)"
            nativeDeltaKb < -NATIVE_THRESHOLD_KB ->
                "native heap fell %,d KB from the post-warm-up baseline (memory released; not growth)".format(-nativeDeltaKb)
            nativeGrewAboveThreshold(nativeDeltaKb, iterations) && trend != null && trend.bytesPerIteration != null && !trend.steadyGrowth ->
                ("native heap ended %+,d KB above baseline (%+.1f bytes per iteration start to end), but the second-half trend is " +
                    "%+.2f bytes per iteration: a one-off step, not steady growth").format(nativeDeltaKb, perIter, trend.bytesPerIteration)
            nativeGrewAboveThreshold(nativeDeltaKb, iterations) ->
                "native heap grew %+,d KB (%+.1f bytes per iteration), above the diagnostic threshold of %d KB and %.0f byte per iteration"
                    .format(nativeDeltaKb, perIter, NATIVE_THRESHOLD_KB, NATIVE_THRESHOLD_BYTES_PER_ITERATION)
            trend != null && trend.steadyGrowth ->
                "native heap change %+,d KB is within the start-to-end threshold, but the second-half trend is %+.2f bytes per iteration: steady growth"
                    .format(nativeDeltaKb, trend.bytesPerIteration)
            else ->
                "native heap change %+,d KB (%+.1f bytes per iteration) is within the diagnostic threshold of %d KB and %.0f byte per iteration"
                    .format(nativeDeltaKb, perIter, NATIVE_THRESHOLD_KB, NATIVE_THRESHOLD_BYTES_PER_ITERATION)
        }
        val drops = if (rssDrops > 0)
            "; RSS fell by more than %,d KB between samples %d time(s), so per-category changes across those points are not comparable with the baseline"
                .format(RSS_DROP_REPORT_KB, rssDrops)
        else ""
        return main + drops
    }

    /** Observation line for an RSS fall between two samples. States what was measured, not why. */
    fun rssDropLine(fromIteration: Int, toIteration: Int, rssDeltaKb: Long, javaDeltaKb: Long,
                    nativeDeltaKb: Long, codeDeltaKb: Long, systemDeltaKb: Long, totalPssDeltaKb: Long): String =
        ("  observed: RSS fell %,d KB between iterations %d and %d (java %+,d, native %+,d, code %+,d, sys %+,d, total PSS %+,d KB). " +
            "The cause was not verified; memory reclaim by Android is one possible explanation.")
            .format(-rssDeltaKb, fromIteration, toIteration, javaDeltaKb, nativeDeltaKb, codeDeltaKb, systemDeltaKb, totalPssDeltaKb)
}
