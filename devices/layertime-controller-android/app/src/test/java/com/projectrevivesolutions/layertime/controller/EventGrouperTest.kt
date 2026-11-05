package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.link.LinkConstants
import com.projectrevivesolutions.layertime.controller.model.EventRow
import com.projectrevivesolutions.layertime.controller.model.RssiSample
import com.projectrevivesolutions.layertime.controller.state.EventGrouper
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * Build 74: one row per MAC address. The field test of 2026-11-01 had the same Apple
 * devices fill DETECTION EVENTS, because the Hunter re-creates a record (new event id)
 * after its 40-record history pushes it out.
 */
class EventGrouperTest {
    private val AIRTAG = 10
    private val FLOCK = 8

    private fun rec(id: Long, t: Long, count: Int = 1, conf: Int = 0, det: Int = AIRTAG, rssi: Int = -60,
                    hasMac: Boolean = true) = EventRow(
        sessionId = 1, eventId = id, detector = det, detectorName = LinkConstants.ReconTarget.name(det),
        confidence = conf, sourceKind = 2, band = 0, channel = 0, rssi = rssi, count = count, ageSeconds = 0,
        hasSourceId = hasMac, hasDetail = false, firstSeenMillis = t, lastUpdatedMillis = t,
        rssiHistory = listOf(RssiSample(t, rssi)),
    )

    private fun EventGrouper.seen(id: Long, mac: String?, t: Long, count: Int = 1, conf: Int = 0, det: Int = AIRTAG) {
        val prev = record(id)
        onRecord(rec(id, t, count, conf, det, hasMac = mac != null).copy(
            sourceId = prev?.sourceId, firstSeenMillis = prev?.firstSeenMillis ?: t), t)
        if (mac != null && prev?.sourceId == null) onText(id, LinkConstants.TextField.SOURCE_ID, mac, t + 1)
    }

    @Test
    fun fiveHundredSightingsOfOneTagAreOneRow() {
        val g = EventGrouper()
        for (i in 1..500) g.seen(7, "AA:BB:CC:DD:EE:01", i * 100L, count = i)
        assertEquals(1, g.rowCount())
        assertEquals(500, g.rows()[0].count)
    }

    @Test
    fun aMacTheHunterDroppedAndRecreatedStaysOneRow() {
        val g = EventGrouper()
        g.seen(3, "aa:bb:cc:dd:ee:01", 1000, count = 120)   // record 3, then pushed out
        g.seen(51, "AA:BB:CC:DD:EE:01", 9000, count = 30)   // same tag, new record, upper case
        g.seen(97, "AA:BB:CC:DD:EE:01", 20000, count = 5)
        val rows = g.rows()
        assertEquals(1, rows.size)
        assertEquals(3L, rows[0].eventId)                    // keeps its first identity
        assertEquals(155, rows[0].count)
        assertEquals(listOf(3L, 51L, 97L), rows[0].mergedEventIds)
        assertEquals(1000L, rows[0].firstSeenMillis)
    }

    @Test
    fun twoMacsAreTwoRows() {
        val g = EventGrouper()
        g.seen(1, "AA:BB:CC:DD:EE:01", 1000)
        g.seen(2, "AA:BB:CC:DD:EE:02", 1100)
        assertEquals(2, g.rowCount())
    }

    @Test
    fun aRecordIsHeldBackUntilItsMacArrives() {
        val g = EventGrouper()
        g.seen(1, "AA:BB:CC:DD:EE:01", 1000)
        // A re-created record for the same tag: shown nowhere until its MAC arrives,
        // so it never flashes up as a second row or a second alert.
        val c = g.onRecord(rec(40, 5000), 5000)
        assertEquals(0, c.newRows.size)
        assertEquals(1, g.rowCount())
        val c2 = g.onText(40, LinkConstants.TextField.SOURCE_ID, "AA:BB:CC:DD:EE:01", 5050)
        assertEquals(0, c2.newRows.size)
        assertEquals(1, g.rowCount())
        assertEquals(listOf(1L, 40L), g.rows()[0].mergedEventIds)
    }

    @Test
    fun aMacThatNeverArrivesIsShownOnItsOwnAfterTheWait() {
        val g = EventGrouper()
        g.onRecord(rec(5, 1000), 1000)
        assertEquals(0, g.rowCount())
        assertEquals(0, g.flush(1000 + EventGrouper.PENDING_TIMEOUT_MS - 1).newRows.size)
        assertEquals(1, g.flush(1000 + EventGrouper.PENDING_TIMEOUT_MS).newRows.size)
        assertEquals(1, g.rowCount())
        // ... and joins its MAC's row if the MAC turns up after all.
        g.seen(6, "AA:BB:CC:DD:EE:01", 9000)
        g.onText(5, LinkConstants.TextField.SOURCE_ID, "AA:BB:CC:DD:EE:01", 9500)
        assertEquals(1, g.rowCount())
        assertEquals(listOf(5L, 6L), g.rows()[0].mergedEventIds)
    }

    @Test
    fun aRecordWithNoMacIsItsOwnRow() {
        val g = EventGrouper()
        g.onRecord(rec(1, 1000, hasMac = false), 1000)
        g.onRecord(rec(2, 1100, hasMac = false), 1100)
        assertEquals(2, g.rowCount())
        g.onText(3, LinkConstants.TextField.SOURCE_ID, "", 1200)   // unknown id: ignored
        g.onRecord(rec(3, 1300), 1300)
        g.onText(3, LinkConstants.TextField.SOURCE_ID, "", 1350)    // empty MAC: own row
        assertEquals(3, g.rowCount())
    }

    @Test
    fun oneMacMatchedByTwoDetectorsIsOneRowNamingBoth() {
        val g = EventGrouper()
        g.seen(1, "AA:BB:CC:DD:EE:01", 1000, conf = 0, det = AIRTAG)
        g.seen(2, "AA:BB:CC:DD:EE:01", 1100, conf = 2, det = FLOCK)
        val r = g.rows().single()
        assertEquals(LinkConstants.ReconTarget.name(AIRTAG) + " + " + LinkConstants.ReconTarget.name(FLOCK), r.detectorName)
        assertEquals(FLOCK, r.detector)                        // the strongest match leads
        assertEquals(2, r.confidence)
    }

    @Test
    fun aRiseInConfidenceIsReportedOnceAndKept() {
        val g = EventGrouper()
        g.seen(1, "AA:BB:CC:DD:EE:01", 1000, conf = 0)
        val up = g.onRecord(rec(1, 2000, conf = 2).copy(sourceId = "AA:BB:CC:DD:EE:01", firstSeenMillis = 1000), 2000)
        assertEquals(1, up.raised.size)
        assertEquals(0, up.raised[0].first)
        val same = g.onRecord(rec(1, 3000, conf = 2).copy(sourceId = "AA:BB:CC:DD:EE:01", firstSeenMillis = 1000), 3000)
        assertEquals(0, same.raised.size)
        // A later Low record of the same MAC never lowers the row.
        g.seen(9, "AA:BB:CC:DD:EE:01", 4000, conf = 0)
        assertEquals(2, g.rows().single().confidence)
    }

    @Test
    fun theRowShowsTheLatestSignalAndAMergedHistory() {
        val g = EventGrouper()
        g.onRecord(rec(1, 1000, rssi = -80).copy(sourceId = "M"), 1000)
        g.onRecord(rec(2, 2000, rssi = -50).copy(sourceId = "M"), 2000)
        val r = g.rows().single()
        assertEquals(-50, r.rssi)
        assertEquals(listOf(-80, -50), r.rssiHistory.map { it.rssi })
        assertTrue(r.rssiHistory.size <= RssiSample.KEEP)
    }

    @Test
    fun rowsAreNewestFirstAndClearEmptiesEverything() {
        val g = EventGrouper()
        g.seen(1, "A", 1000); g.seen(2, "B", 2000); g.seen(1, "A", 3000)
        assertEquals(listOf("A", "B"), g.rows().map { it.sourceId })
        g.clear()
        assertEquals(0, g.rowCount())
    }
}
