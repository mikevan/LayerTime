package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.model.EventRow
import com.projectrevivesolutions.layertime.controller.service.AlertSelector
import org.junit.Assert.assertEquals
import org.junit.Test

/** Build 74: one alert per row (one MAC), and a Low row that rises alerts then. */
class AlertSelectorTest {
    private fun row(id: Long, conf: Int, session: Int = 4) = EventRow(
        sessionId = session, eventId = id, detector = 10, detectorName = "AirTag", confidence = conf,
        sourceKind = 2, band = 0, channel = 0, rssi = -60, count = 1, ageSeconds = 0,
        hasSourceId = true, hasDetail = false, firstSeenMillis = 0, lastUpdatedMillis = 0,
    )

    @Test
    fun aRowAlertsOnceHoweverOftenItUpdates() {
        var st = AlertSelector.State()
        val first = AlertSelector.select(listOf(row(3, 2)), 4, st); st = first.state
        assertEquals(listOf(3L), first.fresh.map { it.eventId })
        repeat(500) { st = AlertSelector.select(listOf(row(3, 2)), 4, st).also { assertEquals(0, it.fresh.size) }.state }
    }

    @Test
    fun lowNeverAlertsButARiseToHighDoes() {
        var r = AlertSelector.select(listOf(row(3, 0)), 4, AlertSelector.State())
        assertEquals(0, r.fresh.size)
        r = AlertSelector.select(listOf(row(3, 2)), 4, r.state)
        assertEquals(listOf(3L), r.fresh.map { it.eventId })
        r = AlertSelector.select(listOf(row(3, 2)), 4, r.state)
        assertEquals(0, r.fresh.size)
    }

    @Test
    fun aNewRowAlertsAndTheOldOneDoesNot() {
        var r = AlertSelector.select(listOf(row(3, 2)), 4, AlertSelector.State())
        r = AlertSelector.select(listOf(row(3, 2), row(9, 1)), 4, r.state)
        assertEquals(listOf(9L), r.fresh.map { it.eventId })
    }

    @Test
    fun aNewSessionStartsOver() {
        var r = AlertSelector.select(listOf(row(3, 2)), 4, AlertSelector.State())
        r = AlertSelector.select(listOf(row(3, 2, session = 5)), 5, r.state)
        assertEquals(listOf(3L), r.fresh.map { it.eventId })
    }

    @Test
    fun aHunterRestartThatReusedTheSessionNumberStartsOver() {
        var r = AlertSelector.select(listOf(row(40, 2)), 4, AlertSelector.State())
        r = AlertSelector.select(listOf(row(2, 2)), 4, r.state)
        assertEquals(listOf(2L), r.fresh.map { it.eventId })
    }

    @Test
    fun theUpgradeFromBuild73DoesNotRealertWhatItAlerted() {
        val legacy = AlertSelector.State(session = 4, maxId = 20, alertedThrough = 20)
        val r = AlertSelector.select(listOf(row(12, 2), row(25, 2)), 4, legacy)
        assertEquals(listOf(25L), r.fresh.map { it.eventId })
    }

    @Test
    fun theRememberedSetIsCapped() {
        var st = AlertSelector.State()
        val rows = (1L..(AlertSelector.MAX_REMEMBERED + 10L)).map { row(it, 2) }
        st = AlertSelector.select(rows, 4, st).state
        assertEquals(AlertSelector.MAX_REMEMBERED, st.alerted.size)
    }
}
