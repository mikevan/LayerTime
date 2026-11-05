package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.wireshark.WiresharkFilter
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class WiresharkFilterTest {
    private val title = "802.11 Deauthentication"
    private val addr = "SA AA:BB:CC:DD:EE:FF  DA FF:FF:FF:FF:FF:FF"
    private val fields = listOf("wlan.fc.type_subtype 0x000c (Deauthentication)", "wlan.fixed.reason_code 7")

    @Test fun blankMatchesEverything() {
        assertTrue(WiresharkFilter.matches(title, addr, fields, ""))
        assertTrue(WiresharkFilter.matches(title, addr, fields, "   "))
    }
    @Test fun substringMatchesTitleAddressOrField() {
        assertTrue(WiresharkFilter.matches(title, addr, fields, "deauth"))
        assertTrue(WiresharkFilter.matches(title, addr, fields, "aa:bb"))
        assertTrue(WiresharkFilter.matches(title, addr, fields, "reason_code"))
        assertTrue(WiresharkFilter.matches(title, addr, fields, "0x000c"))
    }
    @Test fun caseInsensitive() {
        assertTrue(WiresharkFilter.matches(title, addr, fields, "DEAUTH"))
        assertTrue(WiresharkFilter.matches(title, addr, fields, "Aa:Bb"))
    }
    @Test fun termsAreAnded() {
        assertTrue(WiresharkFilter.matches(title, addr, fields, "deauth aa:bb"))
        assertFalse(WiresharkFilter.matches(title, addr, fields, "deauth 11:22"))
    }
    @Test fun noMatchReturnsFalse() {
        assertFalse(WiresharkFilter.matches(title, addr, fields, "beacon"))
    }
}
