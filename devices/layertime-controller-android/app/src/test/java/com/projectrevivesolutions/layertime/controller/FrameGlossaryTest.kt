package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.wireshark.FrameFacts
import com.projectrevivesolutions.layertime.controller.wireshark.FrameGlossary
import com.projectrevivesolutions.layertime.controller.wireshark.WiresharkDecoder
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

/** The Wireshark window's frame explanations (2026-11-04). */
class FrameGlossaryTest {
    // Every name in Wireshark 4.4.19's frame_type_subtype_vals (packet-ieee80211.c), as a card title shows it.
    private val wiresharkNames = listOf(
        "Association Request", "Association Response", "Reassociation Request", "Reassociation Response",
        "Probe Request", "Probe Response", "Measurement Pilot", "Beacon frame", "ATIM", "Disassociate",
        "Authentication", "Deauthentication", "Action", "Action No Ack", "Aruba Management",
        "Trigger", "TWT Ack", "Beamforming Report Poll", "VHT/HE/EHT/RANGING NDP Announcement", "Control Wrapper",
        "802.11 Block Ack Req", "802.11 Block Ack", "Power-Save poll", "Request-to-send", "Clear-to-send",
        "Acknowledgement", "CF-End (Control-frame)", "CF-End + CF-Ack (Control-frame)",
        "Data", "Data + CF-Ack", "Data + CF-Poll", "Data + CF-Ack + CF-Poll", "Null function (No data)",
        "Acknowledgement (No data)", "CF-Poll (No data)", "CF-Ack/Poll (No data)", "QoS Data",
        "QoS Data + CF-Acknowledgment", "QoS Data + CF-Poll", "QoS Data + CF-Ack + CF-Poll",
        "QoS Null function (No data)", "QoS CF-Poll (No Data)", "QoS CF-Ack + CF-Poll (No data)",
        "Poll", "Service Period Request", "Grant", "DMG Clear-to-send", "DMG Denial-to-send",
        "Grant Acknowledgment", "Sector Sweep", "Sector Sweep Feedback", "Sector Sweep Acknowledgment",
    )

    @Test fun everyWiresharkNameHasAnExplanationInFullSentences() {
        for (n in wiresharkNames) {
            val text = FrameGlossary.explain(FrameFacts(typeName = n))
            assertTrue("no entry for $n", FrameGlossary.BY_NAME.containsKey(n))
            assertTrue("not a sentence: $n", text.endsWith(".") && text.first().isUpperCase())
            assertTrue("decorative dash in $n", !text.contains('—'))
        }
        assertEquals(wiresharkNames.toSet(), FrameGlossary.BY_NAME.keys)
    }

    @Test fun titleSuffixIsDroppedAndUnknownNamesSaySo() {
        assertEquals("Deauthentication", FrameGlossary.typeName("Deauthentication (0x000c)"))
        assertEquals("Beacon frame", FrameGlossary.typeName("Beacon frame"))
        assertEquals("Wireshark names this frame LLC. LayerScope has no explanation for it yet.",
            FrameGlossary.explain(FrameFacts(typeName = "LLC")))
    }

    @Test fun eapolIsExplainedByHandshakeMessage() {
        for (m in 1..4) {
            val text = FrameGlossary.explain(FrameFacts(typeName = "QoS Data", eapol = true, eapolMessage = m))
            assertTrue(text.startsWith("WPA handshake message $m of 4."))
        }
        assertEquals(FrameGlossary.EAPOL_OTHER, FrameGlossary.explain(FrameFacts(typeName = "Data", eapol = true)))
        // Without EAPOL, a QoS Data frame is just data.
        assertEquals(FrameGlossary.BY_NAME["QoS Data"], FrameGlossary.explain(FrameFacts(typeName = "QoS Data")))
    }

    @Test fun factsAreReadFromWiresharkFields() {
        fun fd(name: String, display: String, label: String = "") = WiresharkDecoder.Field(1, name, display, 0, 1, label)
        val f = WiresharkDecoder.PcapFrame(7, 23, "IEEE 802.11 plus radiotap", 60, 60, true, 100, 5, 0, "",
            listOf(fd("wlan.fc.type_subtype", "0x0028", "Type/Subtype: QoS Data (0x0028)"),
                fd("wlan.sa", "aa:bb:cc:00:00:01"), fd("wlan.da", "aa:bb:cc:00:00:02"),
                WiresharkDecoder.Field(0, "eapol", "", 0, 99, "802.1X Authentication"),
                fd("wlan_rsna_eapol.keydes.msgnr", "2", "Message number: 2")))
        val facts = FrameFacts.of(f)
        assertEquals("QoS Data", facts.typeName)
        assertEquals("aa:bb:cc:00:00:01", facts.transmitter)
        assertEquals(true, facts.eapol)
        assertEquals(2, facts.eapolMessage)
        assertTrue(FrameGlossary.explain(facts).startsWith("WPA handshake message 2 of 4."))
        assertEquals(7, FrameFacts.parseNumber("0x0007"))
        assertEquals(2, FrameFacts.parseNumber("Message number: 2"))
        assertEquals(null, FrameFacts.parseNumber("none"))
    }
}
