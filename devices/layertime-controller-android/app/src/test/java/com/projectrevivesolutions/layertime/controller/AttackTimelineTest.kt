package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.wireshark.AttackTimeline
import com.projectrevivesolutions.layertime.controller.wireshark.TimelineFrame
import org.junit.Assert.assertEquals
import org.junit.Test

class AttackTimelineTest {
    private fun f(tx: String, ms: Long, seq: Int?, retry: Boolean = false, reason: Int? = null, sub: String = "Deauthentication") =
        TimelineFrame(tx, ms * 1_000_000L, sub, seq, retry, reason)

    @Test fun ratePerTransmitterAndReasonCodes() {
        val s = AttackTimeline.summarize(listOf(
            f("AA", 0, 10, reason = 7), f("AA", 100, 11, reason = 7), f("AA", 200, 12, reason = 7),
            f("AA", 300, 13, reason = 7), f("AA", 400, 14, reason = 7),
        )).single()
        assertEquals(5, s.frames)
        assertEquals(12.5, s.framesPerSecond, 0.001)
        assertEquals(mapOf(7 to 5), s.reasonCodes)
        assertEquals(0, s.seqAnomalies)
        assertEquals(100.0, s.minGapMs!!, 0.001)
    }

    @Test fun aForgedFrameBreaksTheSequence() {
        val s = AttackTimeline.summarize(listOf(
            f("AA", 0, 10), f("AA", 100, 11), f("AA", 200, 900), f("AA", 300, 12),
        )).single()
        assertEquals(2, s.seqAnomalies)
    }

    @Test fun aRetransmissionIsNotAnAnomaly() {
        val s = AttackTimeline.summarize(listOf(
            f("AA", 0, 10), f("AA", 100, 11), f("AA", 150, 11, retry = true), f("AA", 200, 12),
        )).single()
        assertEquals(0, s.seqAnomalies)
        assertEquals(1, s.retries)
    }

    @Test fun transmittersAreSeparatedAndRankedByVolume() {
        val out = AttackTimeline.summarize(listOf(
            f("BB", 0, 1), f("AA", 0, 1), f("AA", 10, 2), f("AA", 20, 3),
        ))
        assertEquals(listOf("AA", "BB"), out.map { it.transmitter })
        assertEquals(3, out[0].frames)
    }

    @Test fun singleFrameHasNoSpanOrGap() {
        val s = AttackTimeline.summarize(listOf(f("AA", 0, 5))).single()
        assertEquals(0.0, s.spanSeconds, 0.0)
        assertEquals(1.0, s.framesPerSecond, 0.0)
        assertEquals(null, s.minGapMs)
    }
}
