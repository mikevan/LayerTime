package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.events.EventTriage
import com.projectrevivesolutions.layertime.controller.model.EventRow
import com.projectrevivesolutions.layertime.controller.model.RssiSample
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class EventTriageTest {

    private fun row(
        detector: Int = 10, confidence: Int = 0, count: Int = 1,
        history: List<RssiSample> = emptyList(), sourceId: String? = "AA:BB:CC:DD:EE:FF",
    ) = EventRow(
        sessionId = 1, eventId = 1, detector = detector, detectorName = "AIRTAG",
        confidence = confidence, sourceKind = 2, band = 1, channel = 6, rssi = -70,
        count = count, ageSeconds = 0, hasSourceId = sourceId != null, hasDetail = false, sourceId = sourceId,
        firstSeenMillis = 0L, lastUpdatedMillis = 0L, rssiHistory = history,
        mergedEventIds = listOf(1),
    )

    @Test fun lowConfidenceNonDwellIsNoise() {
        assertEquals(EventTriage.Triage.NOISE, EventTriage.classify(row(confidence = 0, count = 1)).triage)
    }

    @Test fun medConfidenceIsKept() {
        assertEquals(EventTriage.Triage.KEEP, EventTriage.classify(row(confidence = 1)).triage)
    }

    @Test fun highConfidenceAwayTagIsKept() {
        // Away-from-owner AirTag is graded HIGH by the Hunter, so it must survive triage.
        assertEquals(EventTriage.Triage.KEEP, EventTriage.classify(row(confidence = 2)).triage)
    }

    @Test fun trackerCountAloneIsNotTracked() {
        // Field test 2026-11-02: stationary nearby Apple devices reached counts of 40 with
        // zero time span. Count is sighting volume, not persistence, so it must stay NOISE.
        val v = EventTriage.classify(row(detector = 10, confidence = 0, count = 40))
        assertEquals(EventTriage.Triage.NOISE, v.triage)
    }

    @Test fun trackerThatLingersIsTracked() {
        val hist = listOf(RssiSample(0L, -70), RssiSample(EventTriage.DWELL_MILLIS, -65))
        assertEquals(EventTriage.Triage.TRACK, EventTriage.classify(row(detector = 14, confidence = 0, count = 1, history = hist)).triage)
    }

    @Test fun nonTrackerLowStaysNoiseEvenIfRepeated() {
        // Deauth(5) is not a tracker; repetition alone does not make it TRACK, and LOW is noise.
        assertEquals(EventTriage.Triage.NOISE, EventTriage.classify(row(detector = 5, confidence = 0, count = 99)).triage)
    }

    @Test fun dwellSpanIsLastMinusFirst() {
        val hist = listOf(RssiSample(1000L, -70), RssiSample(6000L, -60))
        assertEquals(5000L, EventTriage.dwellSpanMillis(row(history = hist)))
    }

    @Test fun dwellSpanZeroForSingleReading() {
        assertEquals(0L, EventTriage.dwellSpanMillis(row(history = listOf(RssiSample(1000L, -70)))))
    }

    @Test fun countsSumToTotal() {
        val rows = listOf(
            row(confidence = 0, count = 1),
            row(confidence = 2),
            row(detector = 10, confidence = 0, count = 1, history = listOf(RssiSample(0L, -70), RssiSample(EventTriage.DWELL_MILLIS, -65))),
        )
        val c = EventTriage.counts(rows)
        assertEquals(1, c[EventTriage.Triage.NOISE])
        assertEquals(1, c[EventTriage.Triage.KEEP])
        assertEquals(1, c[EventTriage.Triage.TRACK])
        assertTrue(EventTriage.TRACKER_DETECTORS.contains(10))
        assertFalse(EventTriage.TRACKER_DETECTORS.contains(5))
    }
}
