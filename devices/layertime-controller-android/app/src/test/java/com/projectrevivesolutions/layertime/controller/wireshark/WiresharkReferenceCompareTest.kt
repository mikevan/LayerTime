package com.projectrevivesolutions.layertime.controller.wireshark

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

/** The in-app comparison against TShark 4.4.19 must accept the real decode and catch any deviation. */
class WiresharkReferenceCompareTest {

    // Field dump as produced on the TCL K24, build 0.1.12 (13), 2026-10-11.
    private val tclDecode = """
        epan 4.4.19, 26 bytes
        wlan.fc.type_subtype     : 0x000c
        wlan.da                  : ff:ff:ff:ff:ff:ff
        wlan.sa                  : 00:11:22:33:aa:bb
        wlan.bssid               : 00:11:22:33:aa:bb
        wlan.seq                 : 7
        wlan.fixed.reason_code   : 0x0007
    """.trimIndent()

    @Test
    fun tclDecodeMatchesAllSix() {
        assertTrue(WiresharkDecoder.mismatchesAgainstReference(tclDecode).isEmpty())
    }

    @Test
    fun wrongReasonCodeIsCaught() {
        val bad = tclDecode.replace("0x0007", "0x0003")
        assertEquals(listOf("wlan.fixed.reason_code expected 0x0007"),
            WiresharkDecoder.mismatchesAgainstReference(bad))
    }

    @Test
    fun absentFieldIsCaught() {
        val bad = tclDecode.lines().filterNot { it.startsWith("wlan.bssid") }.joinToString("\n")
        assertEquals(listOf("wlan.bssid expected 00:11:22:33:aa:bb"),
            WiresharkDecoder.mismatchesAgainstReference(bad))
    }

    @Test
    fun parseTreeReadsOffsetsAndLengths() {
        // Tab-separated records as emitted by nativeDecodeTree: depth name display start length
        val raw = "0\twlan\tIEEE 802.11 Deauthentication\t0\t24\n" +
            "1\twlan.fixed.reason_code\t0x0007\t24\t2\n" +
            "junk line without tabs\n"
        val fields = WiresharkDecoder.parseTree(raw)
        assertEquals(2, fields.size)
        val reason = fields[1]
        assertEquals("wlan.fixed.reason_code", reason.name)
        assertEquals("0x0007", reason.display)
        assertEquals(24, reason.start)
        assertEquals(2, reason.length)
    }

    @Test
    fun pcapBytesHaveClassicHeaderAndFrame() {
        val frame = WiresharkDecoder.DEAUTH_FIXTURE
        val pcap = WiresharkDecoder.pcapBytes(frame)
        // 24-byte global header + 16-byte record header + frame
        assertEquals(24 + 16 + frame.size, pcap.size)
        // magic a1b2c3d4 little-endian
        assertEquals(0xd4.toByte(), pcap[0]); assertEquals(0xc3.toByte(), pcap[1])
        assertEquals(0xb2.toByte(), pcap[2]); assertEquals(0xa1.toByte(), pcap[3])
        // linktype 105 at offset 20
        assertEquals(105.toByte(), pcap[20])
    }
}
