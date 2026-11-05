package com.projectrevivesolutions.layertime.controller.wireshark

/**
 * The attack-timeline analytic (claude/layerscope_analytics_design.md, analytics window 2).
 * Pure, host-tested: it summarizes decoded frames per transmitter into the signals that tell
 * ordinary 802.11 housekeeping from an attack. The Link 0.2 design names sequence jumps and
 * retransmissions as the signs of forged frames, so those are first-class here.
 *
 * Nothing here transmits or claims a response; it only describes what was heard.
 */
data class TimelineFrame(
    val transmitter: String,
    val timeNanos: Long,
    val subtype: String,
    val seq: Int?,
    val retry: Boolean,
    val reasonCode: Int?,
)

data class TransmitterStats(
    val transmitter: String,
    val frames: Int,
    val spanSeconds: Double,
    val framesPerSecond: Double,
    val retries: Int,
    /** Breaks in the modulo-4096 sequence that are not retransmissions: the forged-frame tell. */
    val seqAnomalies: Int,
    val reasonCodes: Map<Int, Int>,
    val subtypes: Map<String, Int>,
    val minGapMs: Double?,
    val medianGapMs: Double?,
)

object AttackTimeline {
    private const val SEQ_MOD = 4096

    fun summarize(frames: List<TimelineFrame>): List<TransmitterStats> =
        frames.groupBy { it.transmitter }.map { (tx, fs) -> statsFor(tx, fs) }
            .sortedWith(compareByDescending<TransmitterStats> { it.frames }.thenBy { it.transmitter })

    private fun statsFor(tx: String, unordered: List<TimelineFrame>): TransmitterStats {
        val fs = unordered.sortedBy { it.timeNanos }
        val span = if (fs.size >= 2) (fs.last().timeNanos - fs.first().timeNanos) / 1e9 else 0.0
        val fps = if (span > 0.0) fs.size / span else fs.size.toDouble()

        var retries = 0
        var anomalies = 0
        var prevSeq: Int? = null
        val gaps = ArrayList<Double>()
        var prevTime: Long? = null
        val reasons = HashMap<Int, Int>()
        val subs = HashMap<String, Int>()

        for (f in fs) {
            if (f.retry) retries++
            f.reasonCode?.let { reasons[it] = (reasons[it] ?: 0) + 1 }
            subs[f.subtype] = (subs[f.subtype] ?: 0) + 1
            prevTime?.let { gaps.add((f.timeNanos - it) / 1e6) }
            prevTime = f.timeNanos
            val cur = f.seq
            if (cur != null && prevSeq != null) {
                val expected = (prevSeq!! + 1) % SEQ_MOD
                val isRetransmit = f.retry && cur == prevSeq
                if (cur != expected && !isRetransmit) anomalies++
            }
            if (cur != null) prevSeq = cur
        }
        val sortedGaps = gaps.sorted()
        val median = if (sortedGaps.isEmpty()) null else sortedGaps[sortedGaps.size / 2]
        return TransmitterStats(
            transmitter = tx, frames = fs.size, spanSeconds = span, framesPerSecond = fps,
            retries = retries, seqAnomalies = anomalies, reasonCodes = reasons, subtypes = subs,
            minGapMs = sortedGaps.firstOrNull(), medianGapMs = median,
        )
    }
}
