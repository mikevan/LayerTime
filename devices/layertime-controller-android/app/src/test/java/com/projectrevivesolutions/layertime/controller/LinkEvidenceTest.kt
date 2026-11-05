package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.link.LinkEvidence
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * The phone's half of the Link 0.2 evidence ops (slice 2, 2026-11-04). The request
 * bytes are pinned to the Hunter's encoder (test/test_link_evidence on the C++ side
 * builds the same frames); the assembler refuses gaps and rebuilds frames exactly.
 */
class LinkEvidenceTest {
    private fun b(vararg v: Int) = ByteArray(v.size) { v[it].toByte() }

    @Test fun requestsMatchTheHuntersEncoder() {
        assertArrayEquals(b(0x06, 0x31, 42, 0, 0, 0), LinkEvidence.getFrames(0x31, 42))
        assertArrayEquals(b(0x07, 0x33, 42, 0, 0, 0, 2, 100, 0), LinkEvidence.getFrameBytes(0x33, 42, 2, 100))
        assertArrayEquals(b(0x07, 1, 0x78, 0x56, 0x34, 0x12, 0, 0x34, 0x12), LinkEvidence.getFrameBytes(1, 0x12345678, 0, 0x1234))
    }

    @Test fun repliesDecodeAsTheHunterEncodesThem() {
        val w = LinkEvidence.decodeWindow(b(0x87, 0x31, 42, 0, 0, 0, 3, 0, 3, 0, 0, 0, 1, 1))!!
        assertEquals(42L, w.eventId); assertEquals(3, w.retained); assertEquals(3, w.seen); assertEquals(1, w.sourceKind); assertTrue(w.collecting)
        // 14 bytes exactly, as the Hunter sends (its host test pins the size too).
        assertEquals(null, LinkEvidence.decodeIndex(b(0x88, 0x31, 2, 6, 0xB9, 0x10, 0x01, 0x10, 0x01, 0x04, 0x9C, 0xFF, 0xFF, 0xFF, 0x00)))
        val j = LinkEvidence.decodeIndex(b(0x88, 0x31, 2, 6, 0xB9, 0x10, 0x01, 0x10, 0x01, 0x04, 0x9C, 0xFF, 0xFF, 0xFF))!!
        assertEquals(2, j.index); assertEquals(6, j.channel); assertEquals(-71, j.rssi); assertEquals(272, j.onAir)
        assertTrue(j.trigger); assertEquals(-100, j.offsetMs)
        assertFalse(LinkEvidence.isEvidenceFrame(b(0x85, 1)))
        assertTrue(LinkEvidence.isEvidenceFrame(b(0x89, 1, 0, 0, 0, 7)))
    }

    private fun idx(i: Int, captured: Int, trigger: Boolean = false, offset: Int = 0) =
        LinkEvidence.Index(1, i, 6, -60, captured, captured, if (trigger) LinkEvidence.FLAG_TRIGGER else 0, offset)

    @Test fun theAssemblerRebuildsFramesFromChunksAndRefusesGaps() {
        val f0 = ByteArray(300) { it.toByte() }; val f1 = ByteArray(30) { (100 + it).toByte() }
        val a = LinkEvidence.Assembler(42)
        a.onWindow(LinkEvidence.Window(1, 42, 2, 2, 0, 0, 1, false))
        a.onIndex(idx(0, 300, trigger = true)); a.onIndex(idx(1, 30, offset = 50))
        for (off in 0 until 300 step 239) a.onBytes(LinkEvidence.Bytes(1, 0, off, f0.copyOfRange(off, minOf(300, off + 239))))
        assertFalse(a.complete())
        assertEquals(1 to 0, a.resumePoint())
        a.onBytes(LinkEvidence.Bytes(1, 1, 5, f1.copyOfRange(5, 30)))   // a gap: refused
        assertEquals(1, a.gaps)
        assertFalse(a.complete())
        val b2 = LinkEvidence.Assembler(42)
        b2.onWindow(LinkEvidence.Window(1, 42, 2, 2, 0, 0, 1, false))
        b2.onIndex(idx(0, 300, trigger = true)); b2.onIndex(idx(1, 30, offset = 50))
        b2.onBytes(LinkEvidence.Bytes(1, 0, 0, f0)); b2.onBytes(LinkEvidence.Bytes(1, 1, 0, f1))
        assertTrue(b2.complete())
        assertArrayEquals(f0, b2.frames()[0].bytes); assertArrayEquals(f1, b2.frames()[1].bytes)
        assertEquals(listOf("frame 1: 0 of 30 bytes"), LinkEvidence.Assembler(1).also {
            it.onWindow(LinkEvidence.Window(1, 1, 1, 1, 0, 0, 1, false)); it.onIndex(idx(1, 30)) }.missing())
    }

    @Test fun thePcapCarriesEachFrameWithRadiotapAndTriggerRelativeTimes() {
        val frames = listOf(
            LinkEvidence.Frame(LinkEvidence.Index(1, 0, 6, -48, 40, 40, 0, -300), ByteArray(40) { 1 }),
            LinkEvidence.Frame(LinkEvidence.Index(1, 1, 149, -72, 1500, 1024, LinkEvidence.FLAG_TRIGGER or LinkEvidence.FLAG_TRUNCATED, 0), ByteArray(1024) { 2 }))
        val p = LinkEvidence.toPcap(frames, 1_700_000_000_500L)
        fun u32(o: Int) = (p[o].toLong() and 0xFF) or ((p[o + 1].toLong() and 0xFF) shl 8) or ((p[o + 2].toLong() and 0xFF) shl 16) or ((p[o + 3].toLong() and 0xFF) shl 24)
        assertEquals(127L, u32(20))
        // record 1 at 24: 300 ms before the trigger
        assertEquals(1_700_000_000L, u32(24)); assertEquals(200_000L, u32(28))
        assertEquals(56L, u32(32)); assertEquals(56L, u32(36))
        assertEquals(-48, p[24 + 16 + 14].toInt())            // radiotap antenna signal
        // record 2: the trigger, cut at 1,024 of 1,500 bytes
        val r2 = 24 + 16 + 56
        assertEquals(1_700_000_000L, u32(r2)); assertEquals(500_000L, u32(r2 + 4))
        assertEquals((16 + 1024).toLong(), u32(r2 + 8)); assertEquals((16 + 1500).toLong(), u32(r2 + 12))
        assertEquals(p.size, r2 + 16 + 16 + 1024)
    }
}
