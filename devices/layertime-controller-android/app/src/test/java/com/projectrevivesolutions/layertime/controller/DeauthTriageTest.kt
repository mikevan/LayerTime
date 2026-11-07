package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.flow.DeauthSteps
import com.projectrevivesolutions.layertime.controller.flow.Engine
import com.projectrevivesolutions.layertime.controller.flow.Fixtures
import com.projectrevivesolutions.layertime.controller.flow.FrameSource
import com.projectrevivesolutions.layertime.controller.flow.Lean
import com.projectrevivesolutions.layertime.controller.flow.RunContext
import com.projectrevivesolutions.layertime.controller.flow.Verdict
import com.projectrevivesolutions.layertime.controller.flow.WorkflowLoader
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

/**
 * The pipeline says what each fixture was built to mean (2026-11-04). Runs the
 * shipped wiring file through the engine, not the checks directly, so the test
 * covers the file, the engine, and the steps together.
 */
class DeauthTriageTest {
    private val workflow by lazy {
        val f = listOf("src/main/assets/workflows/deauth_triage.json", "app/src/main/assets/workflows/deauth_triage.json",
            "devices/layertime-controller-android/app/src/main/assets/workflows/deauth_triage.json").map { File(it) }.first { it.exists() }
        DeauthSteps.register()
        WorkflowLoader.parse(f.readText())
    }

    private fun run(s: Fixtures.Scenario) = Engine.run(workflow, RunContext(s.event, FrameSource { s.frames }))

    @Test fun everyFixtureGetsTheVerdictItWasBuiltFor() {
        val wrong = Fixtures.all().mapNotNull { s ->
            val r = run(s)
            val got = r.finding?.verdict
            if (got != s.expected) "${s.name}: expected ${s.expected} got $got; " + (r.finding?.summary ?: r.steps.last().error) +
                "\n  " + (r.finding?.let { f -> (f.features + f.couldNotRun).joinToString("\n  ") { "${it.step}=${it.lean} ${it.value}" } } ?: "") else null
        }
        assertEquals(emptyList<String>(), wrong)
    }

    @Test fun everyStepRunsAndEveryFeatureHasASentence() {
        for (s in Fixtures.all()) {
            val r = run(s)
            assertEquals(s.name, 9, r.steps.size)
            assertTrue(s.name, r.steps.all { it.error == null })
            for (f in r.finding!!.features + r.finding!!.couldNotRun) assertTrue("${s.name} ${f.step}", f.reason.endsWith("."))
            assertTrue(s.name, r.finding!!.summary.endsWith("."))
        }
    }

    @Test fun aFloodWithoutBeaconsReportsTheChecksItCouldNotRun() {
        val f = run(Fixtures.floodWithoutBeacons()).finding!!
        assertEquals(setOf("seq", "signal", "mfp"), f.couldNotRun.map { it.step }.toSet())
        assertTrue(f.summary, f.summary.endsWith("3 checks could not run."))
        assertEquals(Verdict.LIKELY_ATTACK, f.verdict)
    }

    @Test fun theEvilTwinFindingNamesTheCloneAndTheHandshake() {
        val f = run(Fixtures.floodThenEvilTwin()).finding!!
        val after = f.features.first { it.step == "after" }
        assertEquals(Lean.SUSPICIOUS, after.lean)
        assertTrue(after.reason, after.reason.contains("reappeared") && after.reason.contains("handshake"))
        assertTrue(f.summary, f.summary.contains("handshake"))
        assertTrue(after.frames.isNotEmpty())
    }

    @Test fun theFindingPointsAtRealFrames() {
        val s = Fixtures.broadcastFloodReason7()
        val f = run(s).finding!!
        assertTrue(f.frames.isNotEmpty())
        assertTrue(f.frames.all { it in s.frames.indices })
        // The sequence check's evidence is the forged deauths, not the beacons.
        val seq = f.features.first { it.step == "seq" }
        assertTrue(seq.frames.all { s.frames[it].isDeauth })
        assertEquals(40, seq.frames.size)
    }

    @Test fun noFindingEverClaimsAResponse() {
        for (s in Fixtures.all()) {
            val f = run(s).finding!!
            val text = (listOf(f.summary) + (f.features + f.couldNotRun).map { it.reason }).joinToString(" ").lowercase()
            for (word in listOf("countered", "blocked", "mitigated", "jammed")) assertTrue("${s.name} says $word", !text.contains(word))
        }
    }

    @Test fun theFabricatorWritesARealPcap() {
        val p = Fixtures.singleHousekeepingDeauth().pcap
        assertEquals(0xD4.toByte(), p[0]); assertEquals(0xA1.toByte(), p[3]) // little-endian magic
        assertEquals(127, p[20].toInt())                                        // link type radiotap
        // First record: 16-byte header, then 16 bytes radiotap, then an 802.11 beacon (fc0 = 0x80).
        assertEquals(16, p[24 + 16 + 2].toInt())
        assertEquals(0x80.toByte(), p[24 + 16 + 16])
    }
}
