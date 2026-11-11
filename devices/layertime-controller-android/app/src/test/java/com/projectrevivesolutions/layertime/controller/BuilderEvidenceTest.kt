package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.flow.*
import org.junit.Assert.*
import org.junit.Test

class BuilderEvidenceTest {
    private val ap="00:11:22:33:44:55"
    private val client="02:11:22:33:44:66"
    private fun frame(i:Int,ms:Long,eapol:Boolean=false)=AnalyticFrame(i,ms*1000000,if(eapol) AnalyticFrame.DATA else AnalyticFrame.DEAUTH,ap,client,ap,eapol=eapol)
    @Test fun episodesDoNotCombineSeparatedActivityOrDoubleCountFollowOn() {
        val reports=TargetBehavior.analyze(listOf(frame(0,0),frame(1,100,eapol=true),frame(2,10000),frame(3,10001,eapol=true)))
        assertEquals(2,reports.size)
        assertEquals(listOf(1),reports[0].followOnFrames)
        assertEquals(listOf(3),reports[1].followOnFrames)
        assertEquals(listOf(1L),reports[1].followOnDelaysMs)
        assertFalse(reports[1].fullFollowOnWindowObserved)
    }
    @Test fun malformedAndUntimedFramesCannotEstablishCorrelation() {
        val reports=TargetBehavior.analyze(listOf(frame(0,0),frame(1,10,true).copy(timestampValid=false),frame(2,20,true).copy(malformed=true)))
        assertTrue(reports.single().followOnFrames.isEmpty())
    }
    @Test fun duplicatedChecksKeepTheirOwnBlockIdentitiesAndRulesCanReadMetrics() {
        DeauthSteps.register()
        val workflow=Workflow("Two timelines",1,listOf(Step("source","event_source"),Step("timeline1","target_behavior"),Step("timeline2","target_behavior")),
            listOf(Edge("source","timeline1","frames",DataType.FRAMES),Edge("source","timeline2","frames",DataType.FRAMES)))
        val run=Engine.run(workflow,RunContext(DetectionEvent("deauth",ap,null,null,0),FrameSource { listOf(frame(0,0),frame(1,20000,true)) }))
        val a=run.output("timeline1") as Feature;val b=run.output("timeline2") as Feature
        assertEquals("timeline1",a.step);assertEquals("timeline2",b.step)
        assertEquals(1L,a.metrics["linked_episodes"])
        val rules=VerdictRuleSet(emptyList(),listOf(VerdictRule(Cond.MetricAtLeast("timeline2","linked_episodes",1),Verdict.WATCH,"{timeline2.eapol_frames} linked EAPOL frames.","My posture")))
        val f=VerdictRuleSet.parse(rules.toJson()).evaluate(listOf(a,b))
        assertEquals("My posture",f.assessmentLabel);assertEquals("1 linked EAPOL frames.",f.summary)
    }
    @Test fun settingValidationRejectsFractionsNegativeWindowsAndInvalidAddresses() {
        for (settings in listOf(mapOf("follow_on_ms" to -1L),mapOf("follow_on_ms" to 1.5),mapOf("target_mac" to "invalid"))) {
            val w=Workflow("Invalid",1,listOf(Step("s","event_source"),Step("t","target_behavior",settings)),listOf(Edge("s","t","frames",DataType.FRAMES)))
            assertTrue(WorkflowValidator.validate(w).isNotEmpty())
        }
    }
    @Test fun suppliedCombinedWorkflowValidatesAndKeepsUnrelatedEapolOutOfPosture() {
        DeauthSteps.register()
        val file=listOf("src/main/assets/workflows/behavior_posture.json","app/src/main/assets/workflows/behavior_posture.json").map { java.io.File(it) }.first { it.exists() }
        val workflow=WorkflowLoader.parse(file.readText())
        assertTrue(WorkflowValidator.validate(workflow).isEmpty())
        fun run(frames:List<AnalyticFrame>)=Engine.run(workflow,RunContext(DetectionEvent("deauth",ap,null,null,0),FrameSource { frames })).finding!!
        val unrelated=run(listOf(frame(0,0),frame(1,100,true).copy(da="02:11:22:33:44:88")))
        assertEquals("REVIEW AVAILABLE EVIDENCE",unrelated.assessmentLabel)
        val linked=run(listOf(frame(0,0),frame(1,20000,true)))
        assertEquals("DISCONNECT WITH FOLLOW-ON ACTIVITY",linked.assessmentLabel)
        assertEquals(listOf(1),linked.features.first { it.step=="timeline" }.targetReports.single().followOnFrames)
    }

    @Test fun coverageAndUnmatchedRulesNeverReportSafety() {
        val coverage=EvidenceCoverage("recording",1000,600,true,"truncated",2,1)
        assertTrue(coverage.sentence.contains("600 of 1000"))
        assertTrue(coverage.limitations.any { it.contains("cancelled") })
        val finding=VerdictRuleSet(emptyList(),emptyList()).evaluate(emptyList())
        assertEquals(Verdict.WATCH,finding.verdict)
        assertTrue(finding.assessmentLabel.contains("INSUFFICIENT"))
    }
}
