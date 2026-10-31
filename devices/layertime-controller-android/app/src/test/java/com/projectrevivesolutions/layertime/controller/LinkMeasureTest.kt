package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.link.LinkMeasure
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * The watch side of the Phase 0 BULK frames. Byte layouts match
 * test/test_link_measure on the Node (src/core/link/LinkMeasure).
 */
class LinkMeasureTest {
    private fun bytes(vararg v: Int) = ByteArray(v.size) { v[it].toByte() }

    @Test fun bulkRequestMatchesTheNode() {
        assertArrayEquals(bytes(0xF2, 7, 200, 0, 244), LinkMeasure.bulk(7, 200, 244))
        assertArrayEquals(bytes(0xF2, 1, 0xE8, 0x03, 20), LinkMeasure.bulk(1, 1000, 20))
    }

    private fun dataFrame(seq: Int, size: Int): ByteArray {
        val f = ByteArray(size) { (it and 0xFF).toByte() }
        f[0] = 0xF2.toByte(); f[1] = 9; f[2] = (seq and 0xFF).toByte(); f[3] = (seq shr 8).toByte()
        f[4] = 44; f[5] = 1
        return f
    }

    @Test fun intactDataFramesAreRecognised() {
        val f = dataFrame(0x0102, 244)
        assertTrue(LinkMeasure.dataFrameIntact(f))
        assertEquals(0x0102, LinkMeasure.dataSeq(f))
        f[100] = (f[100].toInt() xor 1).toByte()
        assertFalse(LinkMeasure.dataFrameIntact(f))
        assertTrue(LinkMeasure.dataFrameIntact(dataFrame(0, 20)))
        assertFalse(LinkMeasure.dataFrameIntact(bytes(0xF2, 1, 0)))
        assertFalse(LinkMeasure.dataFrameIntact(bytes(0x84, 1, 0, 0, 0, 0)))
    }

    // Same values as done_frame_round_trips_and_fits_twenty_bytes on the Node.
    @Test fun doneFrameDecodes() {
        val f = bytes(0xF2, 3, 0xFF, 0xFF, 200, 0, 244, 247, 0, 0x70, 0x11, 0x01, 0x00, 0x87, 0xD6, 0x12, 0x00)
        val d = LinkMeasure.decodeDone(f)!!
        assertEquals(3, d.reqId)
        assertEquals(200, d.sent)
        assertEquals(244, d.size)
        assertEquals(247, d.mtu)
        assertEquals(70000L, d.refused)
        assertEquals(1234567L, d.elapsedMs)
        assertFalse(LinkMeasure.dataFrameIntact(f))
        assertNull(LinkMeasure.decodeDone(f.copyOf(16)))
        assertNull(LinkMeasure.decodeDone(dataFrame(0, 17)))
    }

    @Test fun statsUseNearestRank() {
        assertNull(LinkMeasure.stats(emptyList()))
        val s = LinkMeasure.stats((1..100).map { it.toDouble() }.shuffled())!!
        assertEquals(100, s.n)
        assertEquals(1.0, s.min, 0.0)
        assertEquals(50.0, s.median, 0.0)
        assertEquals(95.0, s.p95, 0.0)
        assertEquals(100.0, s.max, 0.0)
        val one = LinkMeasure.stats(listOf(446.0))!!
        assertEquals(446.0, one.median, 0.0)
        assertEquals(446.0, one.p95, 0.0)
    }
}
