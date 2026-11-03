package com.projectrevivesolutions.layertime.controller.events

import com.projectrevivesolutions.layertime.controller.model.EventRow

/**
 * Stage 1 triage: separates signal from the Apple Find My flood so the events
 * list and the export are not drowned by momentary AirTag beacons.
 *
 * Three verdicts:
 *   TRACK  a tracker-family device that stays present over time. A tag that keeps
 *          reappearing across the phone's own observation window is the
 *          counter-surveillance hit, whatever its confidence.
 *   NOISE  a LOW-confidence sighting with no dwell. This is the AirTag chaff: a
 *          Find My beacon from a passing phone or a stranger's AirPods. Away-from-
 *          owner tags are graded MED or HIGH by the Hunter, so they do not land here.
 *   KEEP   everything else: MED or HIGH confidence.
 *
 * Dwell is measured by TIME SPAN only (first to last phone-side reading). The
 * Hunter's sighting count is deliberately NOT used: field test 2026-11-02 showed
 * stationary nearby Apple devices reaching counts of 16 to 40 with zero span, so
 * a count rule promoted the whole flood to TRACK. Count says "heard a lot", not
 * "present over time". The sharper signal, a tag that persists across changes in
 * the operator's position, is the next increment and needs per-event position.
 *
 * Pure and host-tested (EventTriageTest). Thresholds are constants, to be wired
 * to an operator control in a later increment.
 */
object EventTriage {
    const val CONF_LOW = 0
    const val CONF_MED = 1
    const val CONF_HIGH = 2

    // ReconTarget numbers for tracker-family detectors (contracts/vectors/enums.json):
    // Flock(8), AirTag(10), Meta(12), Axon(13), Tile(14), SmartTag(15), GoogleTag(16).
    val TRACKER_DETECTORS = setOf(8, 10, 12, 13, 14, 15, 16)

    // A source present across at least this span of phone-side readings is dwelling.
    const val DWELL_MILLIS = 120_000L

    enum class Triage { TRACK, KEEP, NOISE }

    data class Verdict(val triage: Triage, val reason: String)

    /** Milliseconds between the first and last signal reading; 0 if fewer than two. */
    fun dwellSpanMillis(e: EventRow): Long {
        val h = e.rssiHistory
        return if (h.size >= 2) h.last().timeMillis - h.first().timeMillis else 0L
    }

    fun dwells(e: EventRow): Boolean = dwellSpanMillis(e) >= DWELL_MILLIS

    fun classify(e: EventRow): Verdict {
        if (e.detector in TRACKER_DETECTORS && dwells(e)) {
            return Verdict(Triage.TRACK, "tracker dwell: ${dwellSpanMillis(e) / 1000}s across ${e.count} sightings")
        }
        if (e.confidence <= CONF_LOW) {
            return Verdict(Triage.NOISE, "low confidence, no dwell")
        }
        return Verdict(Triage.KEEP, "confidence ${confName(e.confidence)}")
    }

    fun partition(rows: List<EventRow>): Map<Triage, List<EventRow>> =
        rows.groupBy { classify(it).triage }

    fun counts(rows: List<EventRow>): Map<Triage, Int> = mapOf(
        Triage.TRACK to rows.count { classify(it).triage == Triage.TRACK },
        Triage.KEEP to rows.count { classify(it).triage == Triage.KEEP },
        Triage.NOISE to rows.count { classify(it).triage == Triage.NOISE },
    )

    private fun confName(v: Int) = when (v) { CONF_LOW -> "LOW"; CONF_MED -> "MED"; CONF_HIGH -> "HIGH"; else -> "?($v)" }
}
