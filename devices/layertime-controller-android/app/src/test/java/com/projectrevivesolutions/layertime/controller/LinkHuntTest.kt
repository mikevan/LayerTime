package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.link.LinkHunt
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/** The watch side of the HUNT frames. Byte layouts match test/test_link_hunt on the Node (src/core/link/LinkHunt). */
class LinkHuntTest {
    private fun bytes(vararg v: Int) = ByteArray(v.size) { v[it].toByte() }

    @Test fun requestsMatchTheNode() {
        assertArrayEquals(bytes(0x08, 9, 0xF0, 0, 0, 0, 0, 0, 0, 0), LinkHunt.huntStart(9))
        assertArrayEquals(bytes(0x09, 4), LinkHunt.huntStop(4))
        assertArrayEquals(bytes(0x0A, 5), LinkHunt.getHunt(5))
    }

    @Test fun readingDecodesTheNodeVector() {
        // test_link_hunt: hunt_reading_layout_is_little_endian
        val r = LinkHunt.decodeReading(bytes(0x8A, 3, 0, 0x02, 0x01, 0xD8, 0xDD, 0xC4, 0xD3, 0x04, 0x03, 0, 0x01, 0))!!
        assertEquals(3, r.reqId)
        assertEquals(0x0102, r.readings)
        assertEquals(-40, r.latest)
        assertEquals(-35, r.strongest)
        assertEquals(-60, r.weakest)
        assertEquals(-45, r.mean)
        assertEquals(0x0304, r.msSinceHeard)
        assertTrue(r.hunting)
        assertFalse(r.lost)
    }

    @Test fun noReadingAndNeverHeardAreNull() {
        val r = LinkHunt.decodeReading(bytes(0x8A, 1, 0, 0, 0, 0x80, 0x80, 0x80, 0x80, 0xFF, 0xFF, 0, 0x03, 0))!!
        assertEquals(0, r.readings)
        assertNull(r.latest); assertNull(r.mean); assertNull(r.strongest); assertNull(r.weakest)
        assertNull(r.msSinceHeard)
        assertTrue(r.lost)
    }

    @Test fun wrongSizeOrTypeIsRejected() {
        assertNull(LinkHunt.decodeReading(bytes(0x8A, 1, 0)))
        assertNull(LinkHunt.decodeReading(ByteArray(14).also { it[0] = 0x83.toByte() }))
        assertTrue(LinkHunt.isHuntOp(0x0A)); assertFalse(LinkHunt.isHuntOp(0x03))
    }
}
