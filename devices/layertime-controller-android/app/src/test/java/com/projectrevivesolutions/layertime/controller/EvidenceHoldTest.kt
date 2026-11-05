package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.link.LinkCodec
import com.projectrevivesolutions.layertime.controller.link.LinkConstants
import com.projectrevivesolutions.layertime.controller.mission.EvidenceHold
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

/** MISSION's evidence hold (2026-11-04): limits, presets, text, and the wire command. */
class EvidenceHoldTest {
    @Test fun limitsMatchCore() {
        // src/core/model/MonitorEvent.h: kDefaultEvidenceHoldSec 10, kMaxEvidenceHoldSec 30.
        assertEquals(10, EvidenceHold.DEFAULT_SEC)
        assertEquals(30, EvidenceHold.MAX_SEC)
        assertEquals(listOf(0, 5, 10, 20, 30), EvidenceHold.PRESETS)
        assertTrue(EvidenceHold.PRESETS.all { it in 0..EvidenceHold.MAX_SEC })
    }

    @Test fun normalizeKeepsWhatTheHunterTakesAndDefaultsTheRest() {
        assertEquals(0, EvidenceHold.normalize(0))
        assertEquals(30, EvidenceHold.normalize(30))
        assertEquals(17, EvidenceHold.normalize(17))
        assertEquals(10, EvidenceHold.normalize(31))
        assertEquals(10, EvidenceHold.normalize(-1))
        assertEquals(10, EvidenceHold.normalize(null))
    }

    @Test fun labelsAndSentences() {
        assertEquals("OFF", EvidenceHold.label(0))
        assertEquals("20 S", EvidenceHold.label(20))
        for (sec in EvidenceHold.PRESETS) assertTrue(EvidenceHold.meaning(sec).endsWith("."))
        for (l in EvidenceHold.Link.values()) assertTrue(EvidenceHold.status(l).endsWith("."))
        assertTrue(EvidenceHold.meaning(20).contains("20 seconds"))
        assertTrue(EvidenceHold.meaning(0).startsWith("The Hunter never holds the channel."))
    }

    @Test fun commandBytesMatchTheContract() {
        // contracts/link.md: SetEvidenceHold 16, length 4, seconds u8. Capability bit 8.
        assertEquals(16, LinkConstants.CommandType.SET_EVIDENCE_HOLD)
        assertEquals(0x0100, LinkConstants.Capability.EVIDENCE_HOLD)
        assertArrayEquals(byteArrayOf(0x03, 0x70, 0x10, 0x14), LinkCodec.setEvidenceHold(0x70, 20))
        assertArrayEquals(byteArrayOf(0x03, 0x71, 0x10, 0x00), LinkCodec.setEvidenceHold(0x71, 0))
        assertTrue(LinkConstants.Capability.describe(0x0180).containsAll(listOf("sensorSet", "evidenceHold")))
    }
}
