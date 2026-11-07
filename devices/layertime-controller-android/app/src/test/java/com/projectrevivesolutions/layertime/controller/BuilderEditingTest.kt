package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.flow.ConnectResult
import com.projectrevivesolutions.layertime.controller.flow.DeauthSteps
import com.projectrevivesolutions.layertime.controller.flow.Draft
import com.projectrevivesolutions.layertime.controller.flow.DraftEdits
import com.projectrevivesolutions.layertime.controller.flow.WorkflowLoader
import com.projectrevivesolutions.layertime.controller.flow.WorkflowValidator
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * The Builder's goal (Michael, 2026-11-06): rebuild the deauth triage by hand from the
 * palette and get the same verdicts. This test does exactly what the operator does:
 * blank workflow, drop each step, wire each connection, then check the test scenarios.
 */
class BuilderEditingTest {
    private fun add(d: Draft, type: String): Pair<Draft, String> = DraftEdits.add(d, type, 0f, 0f)!!

    private fun wire(d: Draft, from: String, to: String, port: String): Draft {
        val r = DraftEdits.connect(d, from, to, port)
        assertTrue("connect $from -> $to.$port: $r", r is ConnectResult.Done)
        return (r as ConnectResult.Done).draft
    }

    private fun rebuiltTriage(): Draft {
        var d = DraftEdits.blank("My deauth triage")
        val ids = HashMap<String, String>()
        for (t in listOf("event_source", "deauth_target", "deauth_rate", "deauth_reason", "seq_integrity",
                "signal_match", "aftermath", "mgmt_protection", "deauth_verdict")) {
            val (n, id) = add(d, t); d = n; ids[t] = id
        }
        for (t in listOf("deauth_target", "deauth_rate", "deauth_reason", "seq_integrity", "signal_match", "aftermath", "mgmt_protection")) {
            d = wire(d, ids["event_source"]!!, ids[t]!!, "frames")
            d = wire(d, ids[t]!!, ids["deauth_verdict"]!!, "features")
        }
        return d
    }

    @Test fun aHandBuiltTriageGetsEveryScenarioRight() {
        DeauthSteps.register()
        val d = rebuiltTriage()
        assertEquals(emptyList<String>(), DraftEdits.sentences(d.workflow))
        val check = DraftEdits.checkScenarios(d.workflow)
        assertEquals(check.sentence, check.total, check.passed)
        assertEquals(7, check.total)
        assertEquals("Passes all 7 test scenarios.", check.sentence)
    }

    @Test fun anUnfinishedFlowSaysWhatIsMissingInSentences() {
        var d = DraftEdits.blank()
        val (d1, ev) = add(d, "event_source"); d = d1
        val (d2, tg) = add(d, "deauth_target"); d = d2
        val s = DraftEdits.sentences(d.workflow)
        assertTrue(s.toString(), s.any { it.startsWith("Who it targets: Nothing feeds this input") && it.endsWith(".") })
        d = wire(d, ev, tg, "frames")
        assertEquals(emptyList<String>(), DraftEdits.sentences(d.workflow))
        assertTrue(DraftEdits.checkScenarios(d.workflow).failures.isNotEmpty()) // no verdict, so every scenario fails
    }

    @Test fun badConnectionsAreRefusedInSentences() {
        var d = rebuiltTriage()
        val r = DraftEdits.connect(d, "event", "verdict", "features")
        assertTrue(r is ConnectResult.Refused)
        assertEquals("\"Verdict\" takes feature here, but \"Detection\" produces frames.", (r as ConnectResult.Refused).sentence)
        assertTrue(DraftEdits.connect(d, "rate", "rate", "frames") is ConnectResult.Refused)
        assertTrue(DraftEdits.connect(d, "verdict", "rate", "frames") is ConnectResult.Refused) // finding into frames
        // A one-connection input is re-pointed, not doubled.
        val (d2, ev2) = add(d, "event_source"); d = d2
        d = wire(d, ev2, "rate", "frames")
        assertEquals(1, d.workflow.edgesInto("rate").size)
        assertEquals(ev2, d.workflow.edgesInto("rate").single().from)
    }

    @Test fun shippedFlowsAreReadOnlyUntilCopied() {
        val shipped = DraftEdits.open(rebuiltTriage().workflow, null, shipped = true)
        assertFalse(shipped.editable)
        assertEquals(null, DraftEdits.add(shipped, "deauth_rate", 0f, 0f))
        assertTrue(DraftEdits.connect(shipped, "event", "target", "frames") is ConnectResult.Refused)
        val mine = DraftEdits.copyOf(shipped)
        assertTrue(mine.editable)
        assertEquals("My deauth triage copy", mine.workflow.name)
        assertNotNull(DraftEdits.add(mine, "deauth_rate", 0f, 0f))
    }

    @Test fun removeTakesItsConnectionsAndIdsStayUnique() {
        var d = rebuiltTriage()
        d = DraftEdits.remove(d, "rate")
        assertEquals(null, d.workflow.step("rate"))
        assertTrue(d.workflow.edges.none { it.from == "rate" || it.toStep == "rate" })
        assertEquals("rate", DraftEdits.newId(d.workflow, "deauth_rate"))
        assertEquals("event2", DraftEdits.newId(d.workflow, "event_source"))
    }

    @Test fun theSavedFileCarriesTheLayoutAndStillLoads() {
        val d = DraftEdits.move(rebuiltTriage(), "verdict", 512f, 96f)
        val text = DraftEdits.write(d)
        val w = WorkflowLoader.parse(text)
        assertEquals(emptyList<Any>(), WorkflowValidator.validate(w))
        assertEquals(512f to 96f, DraftEdits.layoutOf(text)!!["verdict"])
        assertEquals(d.workflow.steps.map { it.id }, w.steps.map { it.id })
    }

    @Test fun theWiresharkMenuBuildsTheTriageByAssigningPlaceholders() {
        // What the operator does: drop Detection and Verdict, then drop a Wireshark tile for each
        // check and assign it from the menu, then wire. The result must pass every scenario.
        var d = DraftEdits.blank("Menu-built triage")
        val (de, ev) = add(d, "event_source"); d = de
        val (dv, vr) = add(d, "deauth_verdict"); d = dv
        val checkIds = ArrayList<String>()
        for (type in com.projectrevivesolutions.layertime.controller.flow.StepRegistry.WIRESHARK_CHECKS) {
            val (dn, id) = add(d, com.projectrevivesolutions.layertime.controller.flow.StepRegistry.WIRESHARK_PLACEHOLDER); d = dn
            d = DraftEdits.assign(d, id, type)               // long-hold the menu item
            assertEquals(type, d.workflow.step(id)!!.type)
            checkIds += id
        }
        for (id in checkIds) {
            d = wire(d, ev, id, "frames")
            d = wire(d, id, vr, "features")
        }
        assertEquals(emptyList<String>(), DraftEdits.sentences(d.workflow))
        assertEquals("Passes all 7 test scenarios.", DraftEdits.checkScenarios(d.workflow).sentence)
    }

    @Test fun assigningAPlaceholderKeepsItsIdAndAssigningAgainDropsAStaleEdge() {
        var d = DraftEdits.blank("Reassign")
        val (de, ev) = add(d, "event_source"); d = de
        val (dc, c) = add(d, com.projectrevivesolutions.layertime.controller.flow.StepRegistry.WIRESHARK_PLACEHOLDER); d = dc
        val (dv, vr) = add(d, "deauth_verdict"); d = dv
        d = DraftEdits.assign(d, c, "deauth_rate")
        d = wire(d, ev, c, "frames")                          // the check reads frames
        d = wire(d, c, vr, "features")                        // and feeds the verdict
        assertEquals(1, d.workflow.edgesInto(c).size)
        assertEquals(1, d.workflow.edgesFrom(c).size)
        // Change your mind: back to an unassigned Wireshark tile, which produces nothing and has no
        // ports, so both edges no longer fit and are dropped.
        d = DraftEdits.assign(d, c, com.projectrevivesolutions.layertime.controller.flow.StepRegistry.WIRESHARK_PLACEHOLDER)
        assertEquals(c, d.workflow.step("check")!!.id)         // id kept
        assertEquals(com.projectrevivesolutions.layertime.controller.flow.StepRegistry.WIRESHARK_PLACEHOLDER, d.workflow.step(c)!!.type)
        assertTrue(d.workflow.edgesInto(c).isEmpty())
        assertTrue(d.workflow.edgesFrom(c).isEmpty())
    }
}
