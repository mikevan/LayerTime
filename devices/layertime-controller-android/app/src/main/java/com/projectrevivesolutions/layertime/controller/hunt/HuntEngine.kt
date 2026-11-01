package com.projectrevivesolutions.layertime.controller.hunt

/**
 * HUNT v1, the watch's hunt math (claude/hunt_v1_layervixen_plan.md). Pure Kotlin:
 * no Android, no clock; every call takes the time. Unit-tested (HuntEngineTest).
 *
 * The LayerWand answers GET_HUNT with the LayerVixen's signal since the last poll.
 * This turns those readings into what the hunt screen shows: a smoothed signal,
 * its color band, CLOSING or OPENING, the step trail, and FOUND.
 */

/**
 * The thresholds, in dBm. Defaults until tonight's calibration with the real
 * LayerVixen (at 1 m, 3 m, and in the next room); change them here and nowhere else.
 */
data class HuntBands(
    /** At or above: green, a strong signal. */
    val greenDbm: Int = -65,
    /** At or above (and below green): yellow, a slight signal. Below: red. */
    val yellowDbm: Int = -85,
    /** FOUND when the smoothed signal holds at or above this for foundHoldMs. */
    val foundDbm: Int = -50,
    val foundHoldMs: Long = 2_000,
    /**
     * The smoothed signal is the mean of the readings in this window. 2 s: the LayerWand
     * reports the LayerVixen about twice a second (one short scan each), so a 1 s window
     * would hold only one or two readings.
     */
    val smoothMs: Long = 2_000,
    /** CLOSING or OPENING: the smoothed signal now against trendMs ago. */
    val trendMs: Long = 3_000,
    val trendDb: Int = 3,
    /** No sighting for this long: TARGET LOST (the LayerWand says the same after 5 s). */
    val lostMs: Long = 5_000,
    /** Below this the tone is at its slowest and lowest. */
    val toneFloorDbm: Int = -95,
)

/** NONE: not heard (shown red, "not found"). */
enum class SignalBand { NONE, RED, YELLOW, GREEN }

enum class Trend { NONE, CLOSING, STEADY, OPENING }

enum class HuntPhase {
    /** Waiting for the LayerWand to confirm the hunt. */
    STARTING,
    /** Hunting, and the LayerVixen has not been heard yet: acquiring the signal. */
    SEARCHING,
    TRACKING,
    LOST,
    FOUND,
}

data class HuntView(
    val phase: HuntPhase,
    val band: SignalBand,
    val latestDbm: Int?,
    val smoothedDbm: Int?,
    val strongestDbm: Int?,
    val trend: Trend,
    /**
     * Which of the radar's RINGS rings the signal puts you on: RINGS is the outer ring (the
     * faintest signal heard), 1 is the center ring (at FOUND strength). Null when not heard
     * or lost. A stand-in for distance; it never claims a direction.
     */
    val ring: Int?,
    val secondsSinceHeard: Int?,
    val steps: Int,
    val huntSeconds: Int,
    /** One band per step, oldest first. */
    val trail: List<SignalBand>,
)

/** One beep: how long until the next, and its pitch. */
data class Tone(val intervalMs: Long, val freqHz: Int)

class HuntEngine(val bands: HuntBands = HuntBands()) {
    private data class Sample(val atMs: Long, val mean: Int, val count: Int)

    private var startMs = 0L
    private var wandHunting = false
    private var heardEver = false
    private var lastHeardMs = 0L
    private var wandLost = false
    private var latest: Int? = null
    private var strongest: Int? = null
    private var smoothed: Double? = null
    private val samples = ArrayDeque<Sample>()
    private val history = ArrayDeque<Pair<Long, Double>>()
    private var aboveSinceMs: Long? = null
    private var found = false
    private val trail = ArrayList<SignalBand>()
    private var steps = 0

    fun start(nowMs: Long) {
        startMs = nowMs; wandHunting = false; heardEver = false; lastHeardMs = 0; wandLost = false
        latest = null; strongest = null; smoothed = null; samples.clear(); history.clear()
        aboveSinceMs = null; found = false; trail.clear(); steps = 0
    }

    /** One GET_HUNT reply. mean, latest and strongest are null when readings is 0. */
    fun onReading(readings: Int, mean: Int?, latestDbm: Int?, strongestDbm: Int?, hunting: Boolean, lost: Boolean, nowMs: Long) {
        wandHunting = hunting
        wandLost = lost
        if (readings > 0 && mean != null) {
            samples.addLast(Sample(nowMs, mean, readings))
            heardEver = true
            lastHeardMs = nowMs
            latest = latestDbm ?: mean
            val s = strongestDbm ?: mean
            strongest = strongest?.let { maxOf(it, s) } ?: s
        }
        while (samples.isNotEmpty() && nowMs - samples.first().atMs > bands.smoothMs) samples.removeFirst()
        if (samples.isNotEmpty()) {
            val n = samples.sumOf { it.count }
            smoothed = samples.sumOf { it.mean.toDouble() * it.count } / n
        }
        // With no reading in the window the last smoothed value stands until LOST.
        smoothed?.let { history.addLast(nowMs to it) }
        while (history.isNotEmpty() && nowMs - history.first().first > 2 * bands.trendMs) history.removeFirst()

        val sm = smoothed
        if (!found && !isLost(nowMs) && sm != null && sm >= bands.foundDbm) {
            val since = aboveSinceMs ?: nowMs.also { aboveSinceMs = it }
            if (nowMs - since >= bands.foundHoldMs) found = true
        } else if (!found) {
            aboveSinceMs = null
        }
    }

    /** One step from the step sensor: a trail point in the current band. */
    fun onStep(nowMs: Long) {
        steps++
        trail.add(band(nowMs))
        if (trail.size > MAX_TRAIL) trail.removeAt(0)
    }

    private fun isLost(nowMs: Long) = heardEver && (wandLost || nowMs - lastHeardMs >= bands.lostMs)

    fun band(nowMs: Long): SignalBand {
        val sm = smoothed
        if (!heardEver || sm == null || isLost(nowMs)) return SignalBand.NONE
        return when {
            sm >= bands.greenDbm -> SignalBand.GREEN
            sm >= bands.yellowDbm -> SignalBand.YELLOW
            else -> SignalBand.RED
        }
    }

    private fun trend(nowMs: Long): Trend {
        val now = history.lastOrNull() ?: return Trend.NONE
        val then = history.firstOrNull { now.first - it.first <= bands.trendMs } ?: return Trend.NONE
        if (now.first - then.first < bands.trendMs / 2 || isLost(nowMs)) return Trend.NONE
        val d = now.second - then.second
        return when {
            d >= bands.trendDb -> Trend.CLOSING
            d <= -bands.trendDb -> Trend.OPENING
            else -> Trend.STEADY
        }
    }

    fun phase(nowMs: Long): HuntPhase = when {
        found -> HuntPhase.FOUND
        !heardEver && !wandHunting -> HuntPhase.STARTING
        !heardEver -> HuntPhase.SEARCHING
        isLost(nowMs) -> HuntPhase.LOST
        else -> HuntPhase.TRACKING
    }

    fun view(nowMs: Long): HuntView = HuntView(
        phase = phase(nowMs),
        band = band(nowMs),
        latestDbm = if (isLost(nowMs)) null else latest,
        smoothedDbm = if (isLost(nowMs)) null else smoothed?.let { Math.round(it).toInt() },
        strongestDbm = strongest,
        trend = trend(nowMs),
        ring = ringFor(if (isLost(nowMs)) null else smoothed?.let { Math.round(it).toInt() }),
        secondsSinceHeard = if (heardEver) ((nowMs - lastHeardMs) / 1000).toInt() else null,
        steps = steps,
        huntSeconds = ((nowMs - startMs) / 1000).toInt(),
        trail = trail.toList(),
    )

    /** The ring for a smoothed signal: RINGS at toneFloorDbm or weaker, 1 at foundDbm or stronger. */
    fun ringFor(smoothedDbm: Int?): Int? {
        val sm = smoothedDbm ?: return null
        val t = ((sm - bands.toneFloorDbm).toDouble() / (bands.foundDbm - bands.toneFloorDbm)).coerceIn(0.0, 1.0)
        return RINGS - Math.floor(t * (RINGS - 1) + 1e-9).toInt()
    }

    /** The beep for a view, or null for silence (not heard, lost, or found). */
    fun toneFor(v: HuntView): Tone? {
        val sm = v.smoothedDbm ?: return null
        if (v.phase != HuntPhase.TRACKING) return null
        val t = ((sm - bands.toneFloorDbm).toDouble() / (bands.foundDbm - bands.toneFloorDbm)).coerceIn(0.0, 1.0)
        return Tone(intervalMs = (1_100 - 950 * t).toLong(), freqHz = (500 + 1_100 * t).toInt())
    }

    companion object {
        const val MAX_TRAIL = 400
        /** The radar's rings (Michael, 2026-10-31: ten rings that close in as you get closer). */
        const val RINGS = 10
    }
}
