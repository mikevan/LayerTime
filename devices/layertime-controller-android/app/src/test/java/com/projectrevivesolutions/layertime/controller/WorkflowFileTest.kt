package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.flow.DataType
import com.projectrevivesolutions.layertime.controller.flow.FlowJson
import com.projectrevivesolutions.layertime.controller.flow.StepRegistry
import com.projectrevivesolutions.layertime.controller.flow.WorkflowFileError
import com.projectrevivesolutions.layertime.controller.flow.WorkflowLoader
import com.projectrevivesolutions.layertime.controller.flow.WorkflowValidator
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

/**
 * The wiring file (2026-11-04): the shipped deauth triage loads and validates clean,
 * every kind of broken wiring is refused with the step and port named, and a file
 * survives a write-and-reload. Reads the real asset so the shipped file is the one
 * under test; the fallback path covers a test runner started from the repo root.
 */
class WorkflowFileTest {
    private fun shipped(): String {
        val candidates = listOf("src/main/assets/workflows/deauth_triage.json",
            "app/src/main/assets/workflows/deauth_triage.json",
            "devices/layertime-controller-android/app/src/main/assets/workflows/deauth_triage.json")
        val f = candidates.map { File(it) }.firstOrNull { it.exists() }
            ?: throw AssertionError("deauth_triage.json not found from ${File(".").absolutePath}")
        return f.readText()
    }

    @Test fun theShippedDeauthTriageLoadsAndValidatesClean() {
        val w = WorkflowLoader.parse(shipped())
        assertEquals("Deauth triage", w.name)
        assertEquals(9, w.steps.size)
        assertEquals(14, w.edges.size)
        assertEquals(emptyList<Any>(), WorkflowValidator.validate(w))
        val order = WorkflowValidator.order(w)!!
        assertEquals("event", order.first())
        assertEquals("verdict", order.last())
        assertTrue(order.indexOf("event") < order.indexOf("target"))
    }

    @Test fun everyShippedStepTypeHasOperatorText() {
        for (s in StepRegistry.all()) {
            assertTrue(s.type, s.title.isNotBlank())
            assertTrue(s.type, s.description.endsWith("?") || s.description.endsWith("."))
            for (st in s.settings) assertTrue("${s.type}.${st.key}", st.description.endsWith("."))
        }
    }

    private fun broken(vararg edits: Pair<String, String>): String {
        var t = shipped()
        for ((a, b) in edits) { assertTrue("edit not found: $a", t.contains(a)); t = t.replace(a, b) }
        return t
    }
    private fun problems(json: String) = WorkflowValidator.validate(WorkflowLoader.parse(json)).map { it.toString() }

    @Test fun anUnknownStepTypeIsRefusedByName() {
        val p = problems(broken("\"type\": \"seq_integrity\"" to "\"type\": \"seq_magic\""))
        assertEquals(listOf("seq: This app has no step type \"seq_magic\"."), p)
    }

    @Test fun anUnfedInputIsRefusedWithStepAndPort() {
        val p = problems(broken("    { \"from\": \"event\",  \"to\": \"mfp.frames\",       \"carries\": \"frames\" },\n" to ""))
        assertEquals(listOf("mfp.frames: Nothing feeds this input; \"Protection\" needs frames here."), p)
    }

    @Test fun aTypeMismatchIsRefusedAtBothEnds() {
        // The reason check's feature wired straight into the frames-only protection check.
        val p = problems(broken("{ \"from\": \"event\",  \"to\": \"mfp.frames\",       \"carries\": \"frames\" }"
            to "{ \"from\": \"reason\", \"to\": \"mfp.frames\",       \"carries\": \"feature\" }"))
        assertEquals(listOf("mfp.frames: This input takes frames, but the edge carries feature."), p)
    }

    @Test fun aLyingEdgeIsRefused() {
        val p = problems(broken("{ \"from\": \"event\",  \"to\": \"mfp.frames\",       \"carries\": \"frames\" }"
            to "{ \"from\": \"event\",  \"to\": \"mfp.frames\",       \"carries\": \"event\" }"))
        assertEquals(listOf(
            "event: The edge says it carries event, but \"Detection\" produces frames.",
            "mfp.frames: This input takes frames, but the edge carries event."), p)
    }

    @Test fun twoEdgesIntoASingleInputAreRefused() {
        val p = problems(broken("    { \"from\": \"event\",  \"to\": \"mfp.frames\",       \"carries\": \"frames\" },\n"
            to "    { \"from\": \"event\",  \"to\": \"mfp.frames\",       \"carries\": \"frames\" },\n    { \"from\": \"event\",  \"to\": \"mfp.frames\",       \"carries\": \"frames\" },\n"))
        assertEquals(listOf("mfp.frames: 2 edges feed this input; it takes one."), p)
    }

    @Test fun anUnknownOrMistypedSettingIsRefused() {
        assertEquals(listOf("rate: \"Rate and shape\" has no setting \"burst_frame\"."),
            problems(broken("\"burst_frames\": 6" to "\"burst_frame\": 6")))
        assertEquals(listOf("rate: Setting \"burst_frames\" must be a number."),
            problems(broken("\"burst_frames\": 6" to "\"burst_frames\": \"six\"")))
    }

    @Test fun aLoopIsRefused() {
        // Feed the verdict's finding back into the frames step: no order can run it.
        val w = WorkflowLoader.parse(shipped())
        val looped = w.copy(edges = w.edges + com.projectrevivesolutions.layertime.controller.flow.Edge("verdict", "target", "frames", DataType.FINDING))
        val p = WorkflowValidator.validate(looped).map { it.toString() }
        assertTrue(p.toString(), p.any { it.contains("loops back") })
        assertNull(WorkflowValidator.order(looped))
    }

    @Test fun shapeErrorsNameWhatIsMissing() {
        fun err(json: String) = try { WorkflowLoader.parse(json); "no error" } catch (e: WorkflowFileError) { e.message!! }
        assertEquals("\"name\" is missing", err("{\"version\":1,\"steps\":[],\"edges\":[]}"))
        assertEquals("step \"x\" has no \"type\"", err("{\"name\":\"a\",\"version\":1,\"steps\":[{\"id\":\"x\"}],\"edges\":[]}"))
        assertEquals("edge a -> b: \"to\" must be \"step.port\"",
            err("{\"name\":\"a\",\"version\":1,\"steps\":[],\"edges\":[{\"from\":\"a\",\"to\":\"b\",\"carries\":\"frames\"}]}"))
        assertTrue(err("{\"name\":").startsWith("not valid JSON"))
    }

    @Test fun aFileSurvivesWriteAndReload() {
        val w = WorkflowLoader.parse(shipped())
        val again = WorkflowLoader.parse(WorkflowLoader.write(w))
        assertEquals(w, again)
        assertEquals(listOf("a", 1L, 2.5, true, null, mapOf("k" to "v\n\"q\"")),
            FlowJson.parse(FlowJson.write(listOf("a", 1L, 2.5, true, null, mapOf("k" to "v\n\"q\"")))))
    }
}

class FlowLayoutTest {
    @Test fun deauthTriageLaysOutInThreeColumnsWithTheChecksStacked() {
        val f = java.io.File(listOf("src/main/assets/workflows/deauth_triage.json", "app/src/main/assets/workflows/deauth_triage.json")
            .first { java.io.File(it).exists() })
        val w = WorkflowLoader.parse(f.readText())
        val l = com.projectrevivesolutions.layertime.controller.flow.FlowLayout.of(w)
        assertEquals(0, l.node("event")!!.column)
        assertEquals(1, l.node("target")!!.column)
        assertEquals(1, l.node("mfp")!!.column)
        assertEquals(2, l.node("verdict")!!.column)
        assertEquals(7, l.nodes.count { it.column == 1 })
        assertTrue(l.height > l.width) // seven checks stacked in one column make it taller than wide
        // Every box is distinct ground, and a tap inside a box finds that box.
        for (n in l.nodes) assertEquals(n.step, l.hit(n.cx, n.cy)!!.step)
        assertNull(l.hit(-10f, -10f))
    }
}
