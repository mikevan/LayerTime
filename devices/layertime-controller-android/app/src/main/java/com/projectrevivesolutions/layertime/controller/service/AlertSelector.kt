package com.projectrevivesolutions.layertime.controller.service

import com.projectrevivesolutions.layertime.controller.model.EventRow

/**
 * Which detections raise an alert. Kept apart from [DetectionAlerts] so it can be
 * tested on the JVM. Link confidence values: 0 Low, 1 Medium, 2 High. Low is logged
 * and listed, never alerted. Any other value alerts, including one this build does
 * not know, so a future confidence level can never silence the wrist by accident.
 */
object AlertPolicy {
    const val CONFIDENCE_LOW = 0

    fun worthAnAlert(confidence: Int): Boolean = confidence != CONFIDENCE_LOW
}

/**
 * Decides which rows get an alert, one alert per row per Hunter session (build 74).
 *
 * A row is one MAC address (EventGrouper), identified by its eventId, the lowest Hunter
 * event id merged into it. A row alerts once, the first time it is seen at a confidence
 * worth an alert. A Low row is not marked as alerted, so if the same MAC later rises to
 * Medium or High (an AirTag whose owner has walked away, say) it alerts then.
 *
 * Before build 74 the rule was "an event id above the highest already alerted". Every
 * time the Hunter dropped a record for a busy MAC and re-created it, the new id alerted
 * the same device again.
 *
 * [State] is what DetectionAlerts keeps across restarts, so a reconnect or an app
 * restart does not alert the same rows again.
 */
object AlertSelector {
    data class State(
        val session: Int = Int.MIN_VALUE,
        /** Highest row id seen in [session]; spots a Hunter restart that reused the number. */
        val maxId: Long = -1L,
        /** Row ids already alerted in [session]. */
        val alerted: Set<Long> = emptySet(),
        /** Upgrade from build 73, which kept only the highest id alerted: ids up to this count as alerted. */
        val alertedThrough: Long = -1L,
    )

    data class Result(val fresh: List<EventRow>, val state: State)

    /** Caps the remembered set; the oldest (lowest) ids are forgotten first. */
    const val MAX_REMEMBERED = 4000

    fun select(rows: List<EventRow>, session: Int, prior: State): Result {
        if (rows.isEmpty()) return Result(emptyList(), prior)
        var st = if (prior.session == session) prior else State(session = session)
        val top = rows.maxOf { it.eventId }
        // Ids only grow within a session; if every id is below the highest seen, the
        // Hunter restarted and reused the session number, so start over.
        if (st.maxId >= 0 && top < st.maxId) st = State(session = session)
        val fresh = rows.filter {
            it.sessionId == session && AlertPolicy.worthAnAlert(it.confidence) &&
                it.eventId !in st.alerted && it.eventId > st.alertedThrough
        }.sortedBy { it.eventId }
        var alerted = st.alerted + fresh.map { it.eventId }
        if (alerted.size > MAX_REMEMBERED) alerted = alerted.sorted().takeLast(MAX_REMEMBERED).toSet()
        return Result(fresh, State(session, maxOf(st.maxId, top), alerted, st.alertedThrough))
    }
}
