package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.link.CardClear
import com.projectrevivesolutions.layertime.controller.link.HunterRoster
import com.projectrevivesolutions.layertime.controller.link.LinkCard
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

/** HUNTER ACTIONS (2026-11-06): the Hunter list, the card check, Clear Card, and the LOKMAT-only setting. */
class HunterActionsTest {
    @Test fun failedHunterCanBeRetriedWithoutDisconnectingFirst() {
        assertTrue(HunterRoster.canConnect("AA", "AA", online = false, connecting = false, scanning = false))
        assertFalse(HunterRoster.canConnect("AA", "AA", online = true, connecting = false, scanning = false))
        assertFalse(HunterRoster.canConnect("AA", "AA", online = false, connecting = true, scanning = false))
        assertFalse(HunterRoster.canConnect("AA", "AA", online = false, connecting = false, scanning = true))
        assertTrue(HunterRoster.canConnect("BB", "AA", online = true, connecting = false, scanning = false))
    }

    private fun status(flags: Int) = ByteArray(18).also { it[0] = 0x02; it[7] = flags.toByte() }

    @Test fun scannedHuntersAreOneRowEachSortedByNameAndKeepTheirCardAnswer() {
        var l = HunterRoster.seen(emptyList(), "AA", "Hunter-C5-ZZZZZ", -60)
        l = HunterRoster.seen(l, "BB", "Hunter-C5-BSV72", -50)
        l = HunterRoster.seen(l, "AA", "Hunter-C5-ZZZZZ", -40) // heard again
        assertEquals(listOf("BB", "AA"), l.map { it.address })
        assertEquals(-40, l.first { it.address == "AA" }.rssi)
        l = HunterRoster.withCard(l, "BB", HunterRoster.Card.YES)
        l = HunterRoster.seen(l, "BB", null, -55) // a scan result without a name keeps the name and the card
        assertEquals("Hunter-C5-BSV72", l.first { it.address == "BB" }.name)
        assertEquals(HunterRoster.Card.YES, l.first { it.address == "BB" }.card)
        assertEquals("Hunter", HunterRoster.seen(emptyList(), "CC", "", -70).single().name)
    }

    @Test fun theCardAnswerComesFromTheStatusFlag() {
        assertEquals(HunterRoster.Card.YES, HunterRoster.cardFrom(status(0x00)))
        assertEquals(HunterRoster.Card.NO, HunterRoster.cardFrom(status(0x20)))
        assertEquals(HunterRoster.Card.UNKNOWN, HunterRoster.cardFrom(ByteArray(5)))
        assertEquals(HunterRoster.Card.UNKNOWN, HunterRoster.cardFrom(null))
    }

    @Test fun theHeldHunterIsShownEvenThoughItDoesNotAdvertise() {
        val scanned = listOf(HunterRoster.Hunter("AA", "Hunter-C5-AAAAA", -50, HunterRoster.Card.NO))
        val shown = HunterRoster.withHeld(scanned, "BB", "Hunter-C5-BSV72", false)
        assertEquals(2, shown.size)
        assertEquals(HunterRoster.Card.YES, shown.first { it.address == "BB" }.card)
        // Its live Status wins over the scan's answer; no Status yet leaves the scan's answer.
        assertEquals(HunterRoster.Card.YES, HunterRoster.withHeld(scanned, "AA", null, false).single().card)
        assertEquals(HunterRoster.Card.NO, HunterRoster.withHeld(scanned, "AA", null, null).single().card)
        assertEquals(scanned, HunterRoster.withHeld(scanned, null, null, true))
    }

    @Test fun scanSentencesAreCompleteSentences() {
        val all = listOf(HunterRoster.Scan.Listening(0), HunterRoster.Scan.Listening(1), HunterRoster.Scan.Checking(1, 2, "Hunter-C5-BSV72"),
            HunterRoster.Scan.Done(0), HunterRoster.Scan.Done(1), HunterRoster.Scan.Done(3), HunterRoster.Scan.Failed("Bluetooth is off. Turn it on and tap Scan again."))
        all.forEach { assertTrue(HunterRoster.sentence(it)!!.endsWith(".")) }
        assertEquals("Found 1 Hunter.", HunterRoster.sentence(HunterRoster.Scan.Done(1)))
        assertEquals(null, HunterRoster.sentence(HunterRoster.Scan.Idle))
    }

    @Test fun clearCardBytesAndSentences() {
        assertArrayEquals(byteArrayOf(0x0D, 0x44), LinkCard.cardClear(0x44))
        assertTrue(LinkCard.isCardOp(LinkCard.OP_CARD_CLEAR))
        assertEquals(0x0400, LinkCard.CAP_CARD_CLEAR)
        // The card is erased completely; nothing on the phone is mentioned or touched (2026-11-06).
        assertEquals("Erase everything on Hunter-C5-BSV72's SD card? This cannot be undone.", CardClear.confirmText("Hunter-C5-BSV72"))
        val fresh = listOf("hunter_0001.log")
        assertEquals("Hunter-C5-BSV72's card is clear: 1 file erased. It now holds only a fresh hunter_0001.log.",
            CardClear.sentence(CardClear.State.Done("Hunter-C5-BSV72", 1, fresh, true)))
        assertEquals("Hunter-C5-BSV72's card is clear: 255 or more files erased. It now holds only a fresh hunter_0001.log.",
            CardClear.sentence(CardClear.State.Done("Hunter-C5-BSV72", 255, emptyList(), true)))
        assertEquals("Hunter-C5-BSV72 erased 12 files, but some files on its card would not erase. Tap Clear Card again.",
            CardClear.sentence(CardClear.State.Done("Hunter-C5-BSV72", 12, fresh, false)))
        assertEquals("Hunter-C5-BSV72's card still holds 2 files: hunter_0001.log, hunter_0004.log. Tap Clear Card again.",
            CardClear.sentence(CardClear.State.Done("Hunter-C5-BSV72", 0, listOf("hunter_0001.log", "hunter_0004.log"), false)))
        assertTrue(CardClear.clean(emptyList()) && CardClear.clean(fresh) && !CardClear.clean(listOf("hunter_0002.log")))
        listOf(CardClear.OLD_FIRMWARE, CardClear.PULL_RUNNING, CardClear.REFUSED, CardClear.DROPPED, CardClear.TIMED_OUT, CardClear.NO_LIST)
            .forEach { assertTrue(it.endsWith(".")) }
    }

    @Test fun theSwipeSettingAppliesOnlyToTheLokmatWatch() {
        assertTrue(DeviceKind.isLokmatWatch("LOKMAT", "LOKMAT", "APPLLP 5 MAX"))
        assertTrue(DeviceKind.isLokmatWatch("unknown", "unknown", "LOKMAT APPLLP 5 MAX"))
        assertTrue(DeviceKind.isLokmatWatch(null, null, "appllp 5 max"))
        assertFalse(DeviceKind.isLokmatWatch("samsung", "samsung", "SM-F741U"))
        assertFalse(DeviceKind.isLokmatWatch(null, null, null))
    }
}
