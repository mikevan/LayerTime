package com.projectrevivesolutions.layertime.controller.state

import com.projectrevivesolutions.layertime.controller.link.LinkConstants
import com.projectrevivesolutions.layertime.controller.model.EventRow
import com.projectrevivesolutions.layertime.controller.model.RssiSample

/**
 * Folds the Hunter's event records into one row per MAC address.
 *
 * Why: the Hunter already merges repeat sightings of one emitter into one record, but
 * it holds only 40 records (ReconState::kMaxEvents) and drops the oldest when full.
 * Where many Apple devices are in range, a tag is pushed out, seen again, and comes
 * back as a new record with a new event id. Filed by event id, the same MAC became a
 * new row, a new log line, and a new alert each time (field test, 2026-11-01).
 *
 * The rules:
 *  - A record with a MAC belongs to the row for that MAC, whatever detector matched it.
 *    MACs compare without regard to case.
 *  - A record with no MAC is a row of its own.
 *  - The MAC arrives after the record (GET_TEXT). Until it does, the record is held
 *    back, so a repeat of a known MAC never shows, or alerts, as a new row. If the MAC
 *    has not arrived after [pendingTimeoutMs], the record is shown as a row of its own
 *    rather than lost.
 *  - A row's eventId is the lowest event id merged into it, so the row keeps one identity
 *    for the list, the detail page, and alerts. Its count is the sum of its records'
 *    counts; its confidence is the strongest any record has reached; its signal, channel,
 *    and band come from the most recently updated record.
 *
 * Not thread-safe; the repository calls it under its own lock.
 */
class EventGrouper(private val pendingTimeoutMs: Long = PENDING_TIMEOUT_MS) {

    /** What one call changed, for the diagnostic log. */
    data class Change(
        /** Rows that did not exist before this call. */
        val newRows: List<EventRow> = emptyList(),
        /** Rows whose confidence went up, with the confidence before. */
        val raised: List<Pair<Int, EventRow>> = emptyList(),
        /** True when any row on screen changed, so the list must be republished. */
        val touched: Boolean = false,
    ) {
        operator fun plus(o: Change) = Change(newRows + o.newRows, raised + o.raised, touched || o.touched)
    }

    private val records = HashMap<Long, EventRow>()          // raw Hunter records by eventId
    private val keyOf = HashMap<Long, String>()               // eventId -> row key; absent while pending
    private val members = HashMap<String, MutableList<Long>>() // row key -> eventIds
    private val rows = HashMap<String, EventRow>()
    private val pending = LinkedHashSet<Long>()                // eventIds waiting for their MAC

    /** The raw Hunter record for an event id, so the next summary can keep its text fields. */
    fun record(eventId: Long): EventRow? = records[eventId]

    /** Upsert one Hunter record (already built from an EVENT_SUMMARY, text fields kept). */
    fun onRecord(record: EventRow, now: Long): Change {
        records[record.eventId] = record
        return place(record.eventId, now) + flush(now)
    }

    /** A text field arrived for a record. Unknown event ids are ignored. */
    fun onText(eventId: Long, field: Int, text: String, now: Long): Change {
        val r = records[eventId] ?: return flush(now)
        records[eventId] = when (field) {
            LinkConstants.TextField.SOURCE_ID -> r.copy(sourceId = text, lastUpdatedMillis = now)
            LinkConstants.TextField.DETAIL -> r.copy(detail = text, lastUpdatedMillis = now)
            else -> return flush(now)
        }
        return place(eventId, now) + flush(now)
    }

    /** Shows records whose MAC never arrived. Safe to call on every heartbeat. */
    fun flush(now: Long): Change {
        if (pending.isEmpty()) return Change()
        var change = Change()
        for (id in pending.toList()) change += place(id, now)
        return change
    }

    /** Rows, most recently updated first. */
    fun rows(): List<EventRow> = rows.values.sortedByDescending { it.lastUpdatedMillis }

    fun rowCount(): Int = rows.size

    fun clear() {
        records.clear(); keyOf.clear(); members.clear(); rows.clear(); pending.clear()
    }

    private fun keyFor(r: EventRow, now: Long): String? {
        val mac = r.sourceId?.trim().orEmpty()
        if (mac.isNotEmpty()) return MAC + mac.uppercase()
        val waitingForMac = r.hasSourceId && r.sourceId == null && now - r.firstSeenMillis < pendingTimeoutMs
        return if (waitingForMac) null else EVENT + r.eventId
    }

    private fun place(eventId: Long, now: Long): Change {
        val r = records[eventId] ?: return Change()
        val key = keyFor(r, now)
        if (key == null) {                            // still waiting for its MAC
            if (eventId !in keyOf) pending.add(eventId)
            return Change()
        }
        pending.remove(eventId)
        val old = keyOf[eventId]
        var change = Change(touched = true)
        if (old != null && old != key) {
            // A record shown on its own (its MAC was late) now joins its MAC's row.
            members[old]?.remove(eventId)
            if (members[old].isNullOrEmpty()) { members.remove(old); rows.remove(old) } else rebuild(old)
        }
        keyOf[eventId] = key
        val list = members.getOrPut(key) { mutableListOf() }
        if (eventId !in list) list.add(eventId)
        val before = rows[key]
        val after = rebuild(key)
        if (before == null) change += Change(newRows = listOf(after))
        else if (after.confidence > before.confidence) change += Change(raised = listOf(before.confidence to after))
        return change
    }

    private fun rebuild(key: String): EventRow {
        val rs = members.getValue(key).mapNotNull { records[it] }
        val head = rs.maxByOrNull { it.lastUpdatedMillis }!!
        val strongest = rs.maxWith(compareBy<EventRow> { it.confidence }.thenBy { it.lastUpdatedMillis })
        val names = rs.sortedBy { it.eventId }.map { it.detectorName }.distinct()
        val row = head.copy(
            eventId = rs.minOf { it.eventId },
            detector = strongest.detector,
            detectorName = names.joinToString(" + "),
            confidence = strongest.confidence,
            sourceId = head.sourceId ?: rs.firstNotNullOfOrNull { it.sourceId },
            detail = head.detail ?: rs.firstNotNullOfOrNull { it.detail },
            count = rs.sumOf { it.count },
            firstSeenMillis = rs.minOf { it.firstSeenMillis },
            lastUpdatedMillis = head.lastUpdatedMillis,
            rssiHistory = rs.flatMap { it.rssiHistory }.sortedBy { it.timeMillis }.takeLast(RssiSample.KEEP),
            mergedEventIds = rs.map { it.eventId }.sorted(),
        )
        rows[key] = row
        return row
    }

    companion object {
        /** How long a record waits for its MAC before it is shown on its own. */
        const val PENDING_TIMEOUT_MS = 5_000L
        private const val MAC = "mac:"
        private const val EVENT = "event:"
    }
}
