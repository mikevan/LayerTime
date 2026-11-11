package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.ai.AnalyticAuthoring
import com.projectrevivesolutions.layertime.controller.flow.*
import org.junit.Assert.*
import org.junit.Test
import java.io.File

/** Builder teaching candidates exported through the real typed decoder and engine, without a phone. */
class AiAuthoringTrainingSeedTest {
    private data class Case(val id: String, val family: String, val follow: Long, val gap: Long,
                            val threshold: Long, val expectedLinks: Long)

    @Test fun proposalsMatchPromptSettingsAndExecuteOnKnownFrames() {
        val cases = listOf(
            Case("one_link", "linked_episode_threshold", 30000, 3000, 1, 2),
            Case("two_links", "linked_episode_threshold", 30000, 3000, 2, 2),
            Case("three_links", "linked_episode_threshold", 30000, 3000, 3, 2),
            Case("half_second", "short_follow_on", 500, 3000, 1, 0),
            Case("one_second", "boundary_follow_on", 1000, 3000, 2, 2),
            Case("five_seconds", "long_follow_on", 5000, 3000, 2, 2),
            Case("merged_episode", "episode_segmentation", 30000, 20000, 2, 1),
            Case("split_episode", "episode_segmentation", 30000, 500, 2, 2),
        )
        DeauthSteps.register()
        val frames = BuilderPractice.fixture().built
        val records = cases.map { case ->
            val prompt = "Create a reusable analytic named Disconnect sequence ${case.id}. " +
                "Read the whole imported recording. Group disconnect requests by device and network. " +
                "Look for EAPOL within ${case.follow} milliseconds after a disconnect. " +
                "Separate episodes when the gap exceeds ${case.gap} milliseconds. " +
                "Report a matching sequence when at least ${case.threshold} episodes have related EAPOL. " +
                "Otherwise report that the sequence threshold was not met. Keep addresses configurable and do not claim an attack is proven."
            val rules = VerdictRuleSet(emptyList(), listOf(
                VerdictRule(Cond.MetricAtLeast("target_behavior", "linked_episodes", case.threshold), Verdict.WATCH,
                    "{target_behavior.linked_episodes} observed episodes have related security-exchange frames. This does not establish malicious intent or a completed reconnect.", "MATCHING SEQUENCE"),
                VerdictRule(Cond.Always, Verdict.WATCH,
                    "The selected sequence threshold was not met in this recording; missing observations do not establish safety.", "SEQUENCE THRESHOLD NOT MET")))
            val workflow = Workflow("Disconnect sequence ${case.id}", 1, listOf(
                Step("capture_source", "capture_source"),
                Step("target_behavior", "target_behavior", mapOf("follow_on_ms" to case.follow,
                    "episode_gap_ms" to case.gap, "target_mac" to "", "network_mac" to "")),
                Step("verdict", "deauth_verdict", mapOf("rules" to rules.toJson()))), listOf(
                Edge("capture_source", "target_behavior", "frames", DataType.FRAMES),
                Edge("target_behavior", "verdict", "features", DataType.FEATURE)))
            val proposal = FlowJson.write(linkedMapOf("schemaVersion" to 1L,
                "workflow" to FlowJson.parse(WorkflowLoader.write(workflow))))
            val draft = AnalyticAuthoring.decode(proposal, prompt, "authored-training-candidate", "1")
            assertTrue(draft.editable)
            assertTrue(WorkflowValidator.validate(draft.workflow).isEmpty())
            val request = AnalyticAuthoring.request(prompt)
            val finding = Engine.run(draft.workflow, RunContext(
                DetectionEvent("DEAUTH", BuilderPractice.AP, null, null, frames.first().timeNanos),
                FrameSource { frames })).finding
            assertEquals(case.id, if (case.expectedLinks >= case.threshold) "MATCHING SEQUENCE" else "SEQUENCE THRESHOLD NOT MET",
                finding?.assessmentLabel)
            val reports = TargetBehavior.analyze(frames, case.follow, case.gap)
            assertEquals(case.id, case.expectedLinks, reports.count { it.followOnFrames.isNotEmpty() }.toLong())
            linkedMapOf<String, Any?>("id" to "builder-authoring-v1-${case.id}", "family" to case.family,
                "task" to "CREATE_ANALYTIC", "sourceId" to "layertime-engine-authoring-candidates-v1",
                "labelStatus" to "authored-candidate-typed-decoder-and-fixture-engine-verified",
                "trainingApproved" to false, "outputSchema" to request.outputSchema,
                "expectedFixtureLinkedEpisodes" to case.expectedLinks,
                "messages" to request.messages.map { mapOf("role" to it.role.name.lowercase(), "content" to it.content) } +
                    listOf(mapOf("role" to "assistant", "content" to proposal)))
        }
        System.getenv("LAYERTIME_AI_AUTHORING_EXPORT")?.let { destination ->
            val dir = File(destination); dir.mkdirs()
            File(dir, "candidates.jsonl").writeText(records.joinToString("\n", postfix="\n") { FlowJson.write(it) })
        }
        assertEquals(8, records.size)
    }
}
