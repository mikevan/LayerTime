package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.model.RssiSample
import com.projectrevivesolutions.layertime.controller.ui.SignalAnalysis
import com.projectrevivesolutions.layertime.controller.ui.SignalAnalysis.AddressKind
import com.projectrevivesolutions.layertime.controller.ui.SignalAnalysis.TrendState
import com.projectrevivesolutions.layertime.controller.wireshark.WiresharkDecoder
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNull
import org.junit.Test

class SignalAnalysisTest {
    private fun bytes(vararg v: Int) = ByteArray(v.size) { v[it].toByte() }

    @Test fun parsesBothCasesAndSeparators() {
        assertArrayEquals(bytes(0xB4, 0x1E, 0x52, 0x01, 0x02, 0x03), SignalAnalysis.parseAddress("B4:1E:52:01:02:03"))
        assertArrayEquals(bytes(0xC4, 0x11, 0x22, 0x33, 0x44, 0x55), SignalAnalysis.parseAddress(" c4-11-22-33-44-55 "))
    }

    @Test fun rejectsTextThatIsNotAnAddress() {
        assertNull(SignalAnalysis.parseAddress(null))
        assertNull(SignalAnalysis.parseAddress(""))
        assertNull(SignalAnalysis.parseAddress("AB3-1234"))
        assertNull(SignalAnalysis.parseAddress("B4:1E:52:01:02"))
        assertNull(SignalAnalysis.parseAddress("B4:1E:52:01:02:0G"))
        assertNull(SignalAnalysis.parseAddress("B4:1E:52:01:02:003"))
    }

    @Test fun wifiAddressBitsGiveTheKind() {
        val wifi = SignalAnalysis.SOURCE_WIFI
        assertEquals(AddressKind.GLOBAL, SignalAnalysis.addressKind(bytes(0xB4, 0x1E, 0x52, 1, 2, 3), wifi))
        assertEquals(AddressKind.LOCAL, SignalAnalysis.addressKind(bytes(0x02, 0xC0, 0xCA, 1, 2, 3), wifi))
        assertEquals(AddressKind.LOCAL, SignalAnalysis.addressKind(bytes(0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD), wifi))
        assertEquals(AddressKind.GROUP, SignalAnalysis.addressKind(bytes(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF), wifi))
        assertEquals(AddressKind.GROUP, SignalAnalysis.addressKind(bytes(0x01, 0x00, 0x5E, 0, 0, 1), wifi))
    }

    @Test fun bluetoothNeverClaimsAKindTheLinkDoesNotCarry() {
        assertEquals(AddressKind.BLE_UNKNOWN, SignalAnalysis.addressKind(bytes(0xC4, 0x11, 0x22, 0x33, 0x44, 0x55), SignalAnalysis.SOURCE_BLE))
        assertEquals(AddressKind.BLE_UNKNOWN, SignalAnalysis.addressKind(bytes(0x02, 0, 0, 0, 0, 0), SignalAnalysis.SOURCE_BLE))
    }

    private fun samples(now: Long, vararg pairs: Pair<Long, Int>) = pairs.map { RssiSample(now - it.first, it.second) }

    @Test fun noHistoryGivesNoTrend() { assertNull(SignalAnalysis.trend(emptyList(), 0)) }

    @Test fun fewerThanThreeRecentReadingsIsTooFew() {
        val now = 1_000_000L
        val t = SignalAnalysis.trend(samples(now, 10_000L to -80, 0L to -60), now)!!
        assertEquals(TrendState.TOO_FEW, t.state)
        assertEquals(-60, t.latest)
        assertEquals(-60, t.strongest)
    }

    @Test fun readingsOlderThanTheWindowDoNotCount() {
        val now = 1_000_000L
        val t = SignalAnalysis.trend(samples(now, 120_000L to -90, 90_000L to -90, 5_000L to -60), now)!!
        assertEquals(TrendState.TOO_FEW, t.state)
        assertEquals(1, t.readings)
    }

    @Test fun aRiseOfSixDbIsCloser() {
        val now = 1_000_000L
        val t = SignalAnalysis.trend(samples(now, 30_000L to -80, 20_000L to -80, 0L to -74), now)!!
        assertEquals(TrendState.CLOSER, t.state)
        assertEquals(6, t.changeDb)
    }

    @Test fun aDropOfSixDbIsFarther() {
        val now = 1_000_000L
        assertEquals(TrendState.FARTHER, SignalAnalysis.trend(samples(now, 30_000L to -60, 20_000L to -60, 0L to -66), now)!!.state)
    }

    @Test fun bodyPositionSwingsReadAsSteady() {
        val now = 1_000_000L
        assertEquals(TrendState.STEADY, SignalAnalysis.trend(samples(now, 30_000L to -70, 20_000L to -72, 0L to -66), now)!!.state)
    }

    @Test fun lastHeardAddsTimeSinceTheReport() {
        assertEquals(12L, SignalAnalysis.lastHeardSeconds(2, 50_000L, 60_000L))
        assertEquals(2L, SignalAnalysis.lastHeardSeconds(2, 60_000L, 50_000L))  // clock went backwards: never negative
    }

    @Test fun everyDetectorHasItsOwnAdvice() {
        val fallback = SignalAnalysis.advice(999)
        for (d in 5..16) {
            val (what, todo) = SignalAnalysis.advice(d)
            assertNotEquals("detector $d", fallback, what to todo)
            assert(what.endsWith(".") && todo.endsWith(".")) { "detector $d advice must be complete sentences" }
        }
    }

    @Test fun vendorParseHandlesUtf8AndMissingParts() {
        val v = WiresharkDecoder.parseVendor("FlockSafety\tFlock Safety\t24".toByteArray())!!
        assertEquals("Flock Safety", v.longName)
        assertEquals(24, v.maskBits)
        assertEquals("Société", WiresharkDecoder.parseVendor("S\tSociété\t28".toByteArray(Charsets.UTF_8))!!.longName)
        assertEquals("Short", WiresharkDecoder.parseVendor("Short\t\t36".toByteArray())!!.longName)
        assertNull(WiresharkDecoder.parseVendor(null))
        assertNull(WiresharkDecoder.parseVendor("only\ttwo".toByteArray()))
    }
}
