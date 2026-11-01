package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.link.ConnNotice
import com.projectrevivesolutions.layertime.controller.link.ConnNotice.Tone
import com.projectrevivesolutions.layertime.controller.link.LinkClient
import com.projectrevivesolutions.layertime.controller.model.ConnState
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class ConnNoticeTest {
    @Test fun idleAndConnectedShowNothing() {
        assertNull(ConnNotice.of(ConnState.IDLE, null, false))
        assertNull(ConnNotice.of(ConnState.CONNECTED, "link up, server 0.1", false))
    }

    @Test fun bluetoothOffSaysSoAndWhatToTap() {
        val n = ConnNotice.of(ConnState.FAILED, ConnNotice.BLUETOOTH_OFF, false)!!
        assertEquals("Bluetooth is off. Turn it on, then tap SCAN & CONNECT.", n.text)
        assertEquals(Tone.PROBLEM, n.tone)
    }

    @Test fun locationOffShowsTheLocationMessage() {
        assertEquals(LinkClient.LOCATION_OFF_MESSAGE,
            ConnNotice.of(ConnState.FAILED, LinkClient.LOCATION_OFF_MESSAGE, false)!!.text)
    }

    @Test fun scanTimeoutSaysNoneFound() {
        assertTrue(ConnNotice.of(ConnState.FAILED, "no Hunter found in 20s", true)!!.text
            .startsWith("No Hunter found in 20 seconds."))
    }

    @Test fun scanErrorCarriesTheCode() {
        assertEquals("Android could not start the Bluetooth scan (code 2). Tap SCAN & CONNECT to try again.",
            ConnNotice.of(ConnState.FAILED, "scan failed code=2", false)!!.text)
    }

    @Test fun otherFailuresKeepTheReason() {
        assertEquals("The connection failed (LayerTime service not present). Tap SCAN & CONNECT to try again.",
            ConnNotice.of(ConnState.FAILED, "LayerTime service not present", false)!!.text)
    }

    @Test fun dropDependsOnReconnecting() {
        assertEquals("The link to the Hunter dropped (gattStatus=8). Reconnecting.",
            ConnNotice.of(ConnState.DISCONNECTED, "gattStatus=8", true)!!.text)
        assertEquals("The link to the Hunter dropped. Tap SCAN & CONNECT to connect again.",
            ConnNotice.of(ConnState.DISCONNECTED, null, false)!!.text)
    }

    @Test fun progressStatesAreInfo() {
        for (s in listOf(ConnState.SCANNING, ConnState.CONNECTING, ConnState.DISCOVERING, ConnState.HANDSHAKING)) {
            assertEquals(Tone.INFO, ConnNotice.of(s, null, false)!!.tone)
        }
    }

    @Test fun everyNoticeIsACompleteSentence() {
        val details = listOf(null, ConnNotice.BLUETOOTH_OFF, "no Hunter found in 20s", "scan failed code=1", "x")
        for (s in ConnState.values()) for (d in details) for (r in listOf(false, true)) {
            ConnNotice.of(s, d, r)?.let { assertTrue("$s/$d/$r: ${it.text}", it.text.endsWith(".")) }
        }
    }
}
