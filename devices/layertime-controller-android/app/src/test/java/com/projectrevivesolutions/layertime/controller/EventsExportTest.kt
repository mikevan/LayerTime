package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.events.EventTriage
import com.projectrevivesolutions.layertime.controller.events.EventsExport
import com.projectrevivesolutions.layertime.controller.model.EventRow
import com.projectrevivesolutions.layertime.controller.model.RssiSample
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class EventsExportTest {

    private fun row(
        eventId: Long = 1, detector: Int = 10, detectorName: String = "AIRTAG",
        confidence: Int = 2, sourceId: String? = "AA:BB:CC:DD:EE:FF", detail: String? = null,
        count: Int = 1, history: List<RssiSample> = emptyList(),
    ) = EventRow(
        sessionId = 48439, eventId = eventId, detector = detector, detectorName = detectorName,
        confidence = confidence, sourceKind = 2, band = 1, channel = 6, rssi = -70,
        count = count, ageSeconds = 3, hasSourceId = sourceId != null, hasDetail = detail != null, sourceId = sourceId, detail = detail,
        firstSeenMillis = 0L, lastUpdatedMillis = 0L, rssiHistory = history, mergedEventIds = listOf(eventId),
    )

    @Test fun csvHeaderFirstLine() {
        assertEquals(EventsExport.HEADER.joinToString(","), EventsExport.csv(listOf(row())).substringBefore("\r\n"))
    }

    @Test fun csvOneDataRowPerEvent() {
        val lines = EventsExport.csv(listOf(row(eventId = 1), row(eventId = 2))).split("\r\n").filter { it.isNotEmpty() }
        assertEquals(3, lines.size)
    }

    @Test fun csvQuotesFieldsWithCommasAndQuotes() {
        val csv = EventsExport.csv(listOf(row(detail = "says \"hi\", loudly")))
        assertTrue(csv.contains("\"says \"\"hi\"\", loudly\""))
    }

    @Test fun csvEmptyForNoRowsStillHasHeader() {
        assertEquals(EventsExport.HEADER.joinToString(",") + "\r\n", EventsExport.csv(emptyList()))
    }

    @Test fun csvCarriesTriageColumn() {
        val csv = EventsExport.csv(listOf(row(confidence = 0, count = 1)))
        assertTrue(csv.contains("NOISE,\"low confidence, no dwell\""))
    }

    @Test fun jsonHasCountsAndTotal() {
        val rows = listOf(
            row(confidence = 0, count = 1),
            row(confidence = 2),
            row(detector = 10, confidence = 0, count = 1, history = listOf(RssiSample(0L, -70), RssiSample(EventTriage.DWELL_MILLIS, -65))),
        )
        val json = EventsExport.json(rows, nowMs = 0L)
        assertTrue(json.contains("\"total\": 3"))
        assertTrue(json.contains("\"track\": 1"))
        assertTrue(json.contains("\"keep\": 1"))
        assertTrue(json.contains("\"noise\": 1"))
    }

    @Test fun jsonEscapesDetailAndNullsMissing() {
        val json = EventsExport.json(listOf(row(detail = "line1\nline2", sourceId = null)), nowMs = 0L)
        assertTrue(json.contains("\\n"))
        assertTrue(json.contains("\"sourceId\": null"))
    }

    @Test fun jsonTimestampsAreUtcIso() {
        assertTrue(EventsExport.json(listOf(row()), nowMs = 0L).contains("1970-01-01T00:00:00.000Z"))
    }
}
