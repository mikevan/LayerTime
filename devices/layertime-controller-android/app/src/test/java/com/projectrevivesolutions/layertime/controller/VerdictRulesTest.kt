package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.flow.FrameCount
import com.projectrevivesolutions.layertime.controller.flow.FrameFabricator
import com.projectrevivesolutions.layertime.controller.flow.Summary
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * The counting and summary primitives a user builds a Verdict tile from (2026-11-06).
 * Every knob is set in the fixture, so the test is "the primitive reports exactly what
 * was put in front of it."
 */
class VerdictRulesTest {
    private val AP = "aa:bb:cc:00:00:01"
    private val BCAST = com.projectrevivesolutions.layertime.controller.flow.AnalyticFrame.BROADCAST
    private val CLIENT = "10:20:30:40:50:61"
    private val CH = 6
    private val T0 = 1_700_000_000_000_000_000L
    private val MS = 1_000_000L

    @Test fun identicalFramesGroupByTypeMeaningSourceAndDestination() {
        val f = FrameFabricator()
        // 40 identical broadcast deauths, reason 7, from the AP.
        for (i in 0 until 40) f.deauth(T0 + i * MS, AP, BCAST, AP, reason = 7, seq = 100 + i, channel = CH, rssi = -70)
        // 3 deauths with a different reason: a different group though type, source, and destination match.
        for (i in 0 until 3) f.deauth(T0 + (40 + i) * MS, AP, BCAST, AP, reason = 3, seq = 200 + i, channel = CH, rssi = -70)
        // 5 beacons from the AP: a different type.
        for (i in 0 until 5) f.beacon(T0 + (50 + i) * MS, AP, "HomeNet", CH, -48, 300 + i)
        val g = FrameCount.groups(f.built)
        assertEquals("three distinct groups", 3, g.size)
        assertEquals("largest group first", 40, g[0].count)
        assertEquals("Deauthentication", g[0].typeName)
        assertEquals("reason 7", g[0].meaning)
        assertEquals(AP, g[0].sa)
        assertEquals(BCAST, g[0].da)
        assertEquals("40 x Deauthentication, reason 7, $AP -> $BCAST", g[0].line)
        assertEquals("largest reported", 40, FrameCount.largest(f.built))
    }

    @Test fun beaconsGroupByNetworkNameAndEapolByMessageNumber() {
        val f = FrameFabricator()
        for (i in 0 until 4) f.beacon(T0 + i * MS, AP, "HomeNet", CH, -48, 10 + i)
        f.eapol(T0 + 10 * MS, AP, CLIENT, AP, seq = 20, channel = CH, rssi = -50, msg = 1)
        f.eapol(T0 + 11 * MS, CLIENT, AP, AP, seq = 21, channel = CH, rssi = -50, msg = 2)
        val g = FrameCount.groups(f.built)
        val beacon = g.first { it.typeName == "Beacon" }
        assertEquals("SSID \"HomeNet\"", beacon.meaning)
        assertEquals(4, beacon.count)
        val m1 = g.first { it.meaning == "message 1" }
        val m2 = g.first { it.meaning == "message 2" }
        assertEquals(1, m1.count)
        assertEquals(1, m2.count)
    }

    @Test fun aSummaryTemplateFillsItsBlanks() {
        val vars = mapOf("suspicious" to "3", "counted" to "5", "top" to "40 x Deauthentication, reason 7")
        assertEquals("3 of 5 checks lean suspicious. Largest group: 40 x Deauthentication, reason 7.",
            Summary.fill("{suspicious} of {counted} checks lean suspicious. Largest group: {top}.", vars))
    }

    @Test fun anUnknownBlankStaysVisibleInsteadOfVanishing() {
        assertEquals("You wrote {typo} here.", Summary.fill("You wrote {typo} here.", mapOf("real" to "x")))
        assertTrue(Summary.blanksIn("{a} and {b} and {a}").containsAll(listOf("a", "b")))
        assertEquals("two distinct blanks", 2, Summary.blanksIn("{a} and {b} and {a}").size)
    }

    @Test fun theDefaultRuleSetSurvivesWriteAndReload() {
        val a = com.projectrevivesolutions.layertime.controller.flow.VerdictRuleSet.deauthDefault()
        val b = com.projectrevivesolutions.layertime.controller.flow.VerdictRuleSet.parse(a.toJson())
        assertEquals(a.counted, b.counted)
        assertEquals(a.rules.size, b.rules.size)
        assertEquals(a.rules.map { it.verdict }, b.rules.map { it.verdict })
        assertEquals(a.rules.map { it.summary }, b.rules.map { it.summary })
    }

    @Test fun anOperatorRuleSetCanCountIdenticalFrames() {
        val f = FrameFabricator()
        for (i in 0 until 12) f.deauth(T0 + i * MS, AP, BCAST, AP, reason = 7, seq = 100 + i, channel = CH, rssi = -70)
        val rs = com.projectrevivesolutions.layertime.controller.flow.VerdictRuleSet.parse(
            """{"counted":[],"rules":[
                 {"verdict":"LIKELY ATTACK","summary":"Saw {top}.","when":{"group":"identical","atLeast":10}},
                 {"verdict":"NOT WORRISOME","summary":"Quiet.","when":{"always":true}}]}""")
        val finding = rs.evaluate(emptyList(), f.built)
        assertEquals(com.projectrevivesolutions.layertime.controller.flow.Verdict.LIKELY_ATTACK, finding.verdict)
        assertTrue(finding.summary, finding.summary.contains("12 x Deauthentication, reason 7"))
    }

    @Test fun aBadRuleSetSaysWhatIsWrongInASentence() {
        try {
            com.projectrevivesolutions.layertime.controller.flow.VerdictRuleSet.parse(
                """{"counted":[],"rules":[{"verdict":"NUKE IT","summary":"x","when":{"always":true}}]}""")
            assertTrue("should have thrown", false)
        } catch (e: com.projectrevivesolutions.layertime.controller.flow.VerdictRuleSetError) {
            assertTrue(e.message, e.message!!.contains("is not a verdict"))
        }
    }
}
