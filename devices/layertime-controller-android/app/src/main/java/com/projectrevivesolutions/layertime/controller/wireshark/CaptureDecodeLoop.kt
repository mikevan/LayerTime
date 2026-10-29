package com.projectrevivesolutions.layertime.controller.wireshark

/**
 * The capture-import decode loop, free of Android types so it is unit-tested
 * on the JVM with a fake frame source. Wireshark does the decoding (through
 * [WiresharkDecoder.PcapReader]); this loop only counts, keeps, and reports.
 *
 * Liveness: [onBeat] is called at least every [beatIntervalNs] while frames are
 * being decoded and once at the end, with the frame count, the rate over the
 * last interval, the elapsed time, and the share of the file read so far. A
 * beat is flagged slowing when the interval rate falls below half the first
 * interval's rate.
 */
object CaptureDecodeLoop {

    interface FrameSource : java.io.Closeable {
        val error: String?
        val fileType: String
        val fileSize: Long
        fun next(): WiresharkDecoder.PcapFrame?
    }

    data class Beat(
        val frames: Int, val ratePerSecond: Double, val elapsedSeconds: Double,
        val percentOfFile: Int?, val slowing: Boolean, val firstRate: Double, val last: Boolean
    ) {
        fun line(): String = "HEARTBEAT pcap %,d frames%s %.0f/s elapsed %.0f s%s%s".format(
            frames, percentOfFile?.let { " ($it% of file)" } ?: "", ratePerSecond, elapsedSeconds,
            if (slowing) " SLOWING (now %.0f/s, started at %.0f/s)".format(ratePerSecond, firstRate) else "",
            if (last) " final" else "")
    }

    data class Result(
        val totalFrames: Int,
        val kept: List<WiresharkDecoder.PcapFrame>,
        val keepLimit: Int,
        val cancelled: Boolean,
        val readError: String?,
        val elapsedSeconds: Double,
        val malformedFrames: Int
    )

    /** Tracks rate per interval. Pure; time is passed in. */
    class RateMeter(private val intervalNs: Long, private val startNs: Long) {
        private var lastNs = startNs
        private var lastCount = 0
        private var firstRate = -1.0

        /** Returns a beat when an interval has passed, or when [force] is set; otherwise null. */
        fun tick(count: Int, nowNs: Long, percent: Int?, force: Boolean = false): Beat? {
            if (!force && nowNs - lastNs < intervalNs) return null
            val intervalS = (nowNs - lastNs) / 1e9
            val rate = if (intervalS > 0) (count - lastCount) / intervalS else 0.0
            if (firstRate < 0 && rate > 0 && !force) firstRate = rate
            lastNs = nowNs; lastCount = count
            val slowing = !force && firstRate > 0 && rate < firstRate * 0.5
            return Beat(count, rate, (nowNs - startNs) / 1e9, percent, slowing, firstRate, force)
        }
    }

    fun percentOf(offset: Long, size: Long): Int? =
        if (size <= 0 || offset < 0) null else ((offset * 100) / size).coerceIn(0, 100).toInt()

    fun run(
        source: FrameSource,
        keepLimit: Int,
        cancelled: () -> Boolean,
        nowNs: () -> Long,
        beatIntervalNs: Long = 5_000_000_000L,
        onBeat: (Beat) -> Unit,
        onFrame: (WiresharkDecoder.PcapFrame) -> Unit = {}
    ): Result {
        val start = nowNs()
        val meter = RateMeter(beatIntervalNs, start)
        val kept = ArrayList<WiresharkDecoder.PcapFrame>()
        var count = 0
        var malformed = 0
        var percent: Int? = 0
        var wasCancelled = false
        while (true) {
            if (cancelled()) { wasCancelled = true; break }
            val f = source.next() ?: break
            count++
            if (f.malformed) malformed++
            if (kept.size < keepLimit) kept.add(f)
            percent = percentOf(f.fileOffset, source.fileSize)
            onFrame(f)
            meter.tick(count, nowNs(), percent)?.let(onBeat)
        }
        val end = nowNs()
        meter.tick(count, end, if (wasCancelled) percent else 100.takeIf { source.fileSize > 0 }, force = true)?.let(onBeat)
        val err = if (wasCancelled) null else source.error
        return Result(count, kept, keepLimit, wasCancelled, err, (end - start) / 1e9, malformed)
    }
}
