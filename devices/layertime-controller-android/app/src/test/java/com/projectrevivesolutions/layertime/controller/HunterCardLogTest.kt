package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.card.HunterCardLog
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

/** The Hunter's SD card log as LayerScope reads it (2026-11-04). Lines are the firmware's own test lines. */
class HunterCardLogTest {
    // devices/lilygo-hunter/test/test_tdongle_c5_bringup: the expected lines, verbatim.
    private val log = listOf(
        HunterCardLog.HEADER,
        "7,0,\"boot\",,,,,\"\",\"power-on\",,,,",
        "2,123456,\"event\",17,\"DEAUTH\",\"medium\",\"wifi\",\"AA:BB:CC:DD:EE:FF\",\"Free \"\"WiFi\"\", ok\",-67,36,\"5GHz\",3",
        "2,130000,\"repeat\",17,\"DEAUTH\",\"high\",\"wifi\",\"AA:BB:CC:DD:EE:FF\",\"Free \"\"WiFi\"\", ok\",-55,36,\"5GHz\",9",
        "2,141000,\"evidence\",17,,,,,\"ev_0007/event_00017.pcap kept=49 seen=51 dropped=2 truncated=0 collecting=0\",,,,49",
        "1,5000,\"link\",,,,,\"e0:48:24:3d:a2:60\",\"connected\",,,,",
        "1,6000,\"mode\",,,,,\"\",\"early-warning\",,,,",
        "2,150000,\"event\",18,\"AIRTAG\",\"low\",\"ble\",\"11:22:33:44:55:66\",\"Name \\x07 bell \\\\ slash\",-80,0,\"\",1",
        "garbage line",
    ).joinToString("\r\n")

    @Test fun eventsRepeatsAndEvidenceBecomeOneRowPerDetection() {
        val l = HunterCardLog.parse(log)
        assertEquals(7L, l.bootCount)
        assertEquals("power-on", l.resetReason)
        assertEquals(2, l.detections.size)
        val d = l.detections[0]
        assertEquals(17L, d.eventId)
        assertEquals("DEAUTH", d.detector)
        assertEquals("Free \"WiFi\", ok", d.detail)
        assertEquals(123456L, d.firstMs)
        assertEquals(130000L, d.lastMs)      // from the repeat
        assertEquals(9L, d.count)
        assertEquals(-55, d.rssi)
        assertEquals("high", d.confidence)   // a repeat can raise it
        assertEquals("ev_0007/event_00017.pcap", d.evidencePath)
        assertEquals(49, d.evidenceKept)
        assertEquals(51, d.evidenceSeen)
        assertEquals(2, d.evidenceDropped)
        val b = l.detections[1]
        assertEquals("Name \u0007 bell \\ slash", b.detail)
        assertNull(b.evidencePath)
        assertNull(b.channel?.takeIf { it != 0 })
        assertEquals(1, l.links.size)
        assertEquals(true, l.links[0].connected)
        assertEquals("e0:48:24:3d:a2:60", l.links[0].peer)
        assertEquals(listOf(6000L to "early-warning"), l.modes)
        assertEquals(1, l.unreadLines)       // the garbage line, counted
    }

    @Test fun namesAndFolders() {
        assertEquals(7, HunterCardLog.logSerial("hunter_0007.log"))
        assertEquals(7, HunterCardLog.logSerial("HUNTER_0007.LOG"))
        assertNull(HunterCardLog.logSerial("hunter_7.log"))
        assertNull(HunterCardLog.logSerial("notes.txt"))
        assertEquals("ev_0007", HunterCardLog.evidenceFolder("hunter_0007.log"))
        assertEquals("1 min 21 s after boot", HunterCardLog.sinceBoot(81_000))
        assertEquals("2 h 5 min after boot", HunterCardLog.sinceBoot(7_500_000))
    }

    @Test fun oldLayerWandLogNamesAreReadAndRenamed() {
        assertEquals(7, HunterCardLog.logSerial("layerwand_0007.log"))
        assertEquals("ev_0007", HunterCardLog.evidenceFolder("LAYERWAND_0007.LOG"))
        assertEquals("hunter_0007.log", HunterCardLog.currentName("layerwand_0007.log"))
        assertNull(HunterCardLog.currentName("hunter_0007.log"))
        val dir = java.nio.file.Files.createTempDirectory("card").toFile()
        try {
            java.io.File(dir, "layerwand_0001.log").writeText("old one")
            java.io.File(dir, "layerwand_0002.log").writeText("old two")
            java.io.File(dir, "hunter_0002.log").writeText("new two") // taken: the old one stays
            assertEquals(1, HunterCardLog.renameLegacyLogs(dir))
            assertEquals("old one", java.io.File(dir, "hunter_0001.log").readText())
            assertEquals("new two", java.io.File(dir, "hunter_0002.log").readText())
            assertEquals(true, java.io.File(dir, "layerwand_0002.log").exists())
            assertEquals(0, HunterCardLog.renameLegacyLogs(dir))
        } finally { dir.deleteRecursively() }
    }

    @Test fun aRepeatWithoutItsEventStillShows() {
        val l = HunterCardLog.parse("2,9000,\"repeat\",5,\"DEAUTH\",\"high\",\"wifi\",\"aa\",\"d\",-50,6,\"2.4GHz\",4")
        assertEquals(1, l.detections.size)
        assertEquals(4L, l.detections[0].count)
    }
}
