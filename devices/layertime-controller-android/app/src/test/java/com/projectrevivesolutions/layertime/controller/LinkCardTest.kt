package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.link.CardPull
import com.projectrevivesolutions.layertime.controller.link.LinkCard
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/** Link 0.2 card ops (2026-11-04), pinned to the same bytes as test/test_link_card on the Hunter side. */
class LinkCardTest {
    @Test fun requestBytesMatchTheHunter() {
        assertArrayEquals(byteArrayOf(0x0B, 0x41), LinkCard.cardList(0x41))
        val id = LinkCard.FileId(LinkCard.KIND_PCAP, 0x0102, 0x0A0B0C0DL)
        val want = byteArrayOf(0x0C, 0x42, 1, 0x02, 0x01, 0x0D, 0x0C, 0x0B, 0x0A, 0x44, 0x33, 0x22, 0x11)
        assertArrayEquals(want, LinkCard.cardRead(0x42, id, 0x11223344L))
    }

    @Test fun replyFramesDecode() {
        val entry = byteArrayOf(0x8B.toByte(), 0x43, 1, 0x07, 0x00, 42, 0, 0, 0, 0x18, 0x08, 0x01, 0x00)
        val e = LinkCard.decodeEntry(entry)!!
        assertEquals(0x43, e.reqId)
        assertEquals("ev_0007/event_00042.pcap", e.id.name)
        assertEquals(67608L, e.size)
        assertNull(LinkCard.decodeEntry(entry.copyOf(12)))
        val (offset, bytes) = LinkCard.decodeBytes(byteArrayOf(0x8C.toByte(), 1, 0xF4.toByte(), 1, 0, 0, 9, 8))!!
        assertEquals(500L, offset)
        assertArrayEquals(byteArrayOf(9, 8), bytes)
        val d = LinkCard.decodeDone(byteArrayOf(0x8D.toByte(), 2, 0xE8.toByte(), 3, 0, 0, 0x26, 0x39, 0xF4.toByte(), 0xCB.toByte()))!!
        assertEquals(1000L, d.size)
        assertEquals(0xCBF43926L, d.crc32)
        assertTrue(LinkCard.isCardFrame(byteArrayOf(0x8C.toByte())) && !LinkCard.isCardFrame(byteArrayOf(0x89.toByte())))
    }

    @Test fun namesAndValidityMatchTheHunter() {
        assertEquals("hunter_0007.log", LinkCard.FileId(LinkCard.KIND_LOG, 7, 0).name)
        assertEquals("ev_0007/event_4294967295.pcap", LinkCard.FileId(LinkCard.KIND_PCAP, 7, 4294967295L).name)
        assertTrue(LinkCard.FileId(LinkCard.KIND_LOG, 9999, 0).valid)
        assertTrue(!LinkCard.FileId(LinkCard.KIND_LOG, 0, 0).valid)
        assertTrue(!LinkCard.FileId(LinkCard.KIND_LOG, 7, 3).valid)
        assertTrue(!LinkCard.FileId(LinkCard.KIND_PCAP, 7, 0).valid)
        assertTrue(!LinkCard.FileId(2, 7, 1).valid)
    }

    @Test fun crcIsTheStandardOne() {
        assertEquals(0xCBF43926L, LinkCard.crc32("123456789".toByteArray()))
        assertEquals(0L, LinkCard.crc32(ByteArray(0)))
    }

    @Test fun assemblerTakesChunksInOrderAndChecksTheWholeFile() {
        val file = ByteArray(1000) { ((it * 7 + 3) and 0xFF).toByte() }
        val a = LinkCard.Assembler(LinkCard.FileId(LinkCard.KIND_LOG, 1, 0), 1000)
        a.onBytes(0, file.copyOfRange(0, 400))
        a.onBytes(600, file.copyOfRange(600, 700)) // out of order: refused
        assertEquals(1, a.refused)
        assertEquals(400L, a.received)
        assertTrue(a.problem()!!.contains("did not finish"))
        // A resume continues from received.
        a.onBytes(400, file.copyOfRange(400, 1000))
        a.onDone(LinkCard.Done(1, 1000, LinkCard.crc32(file)))
        assertNull(a.problem())
        assertArrayEquals(file, a.bytes())
        // A wrong checksum is caught.
        val b = LinkCard.Assembler(LinkCard.FileId(LinkCard.KIND_LOG, 1, 0), 1000)
        b.onBytes(0, file)
        b.onDone(LinkCard.Done(1, 1000, 0x12345678L))
        assertTrue(b.problem()!!.contains("checksum"))
    }

    @Test fun aPullIsOfferedOnlyWhenTheHunterHasACard() {
        assertEquals(CardPull.Offer.NoHunter, CardPull.offer(false, null))
        assertEquals(CardPull.Offer.NoHunter, CardPull.offer(false, false))
        assertEquals(CardPull.Offer.Checking, CardPull.offer(true, null))
        assertEquals(CardPull.Offer.Card, CardPull.offer(true, false))
        assertEquals(CardPull.Offer.NoCard, CardPull.offer(true, true))
        assertTrue(CardPull.NO_CARD_SENTENCE.endsWith("."))
    }
}
