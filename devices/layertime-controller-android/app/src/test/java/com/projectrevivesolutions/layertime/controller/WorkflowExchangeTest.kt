package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.flow.*
import org.junit.Assert.*
import org.junit.Test

class WorkflowExchangeTest {
    @Test fun exchangedDraftPreservesWiringSettingsAndLayoutWithoutSelectingIt() {
        var d=DraftEdits.blank("Community posture")
        val added=DraftEdits.add(d,"target_behavior",150f,250f)!!;d=added.first
        d=DraftEdits.setSetting(d,added.second,"target_mac","02:11:22:33:44:55")
        val loaded=WorkflowExchange.read(DraftEdits.write(d).byteInputStream())
        assertEquals(d,loaded)
        assertTrue(loaded.editable)
        assertTrue(DraftEdits.sentences(loaded.workflow).isNotEmpty()) // Missing source is visible, not auto-run.
    }
    @Test(expected=IllegalArgumentException::class) fun oversizedImportIsRejectedBeforeParsing() {
        WorkflowExchange.read(ByteArray(WorkflowExchange.MAX_BYTES+1).inputStream())
    }
    @Test fun missingCheckUnderNotDoesNotBecomeEvidence() {
        val rule=VerdictRule(Cond.Not(Cond.CheckLeans("missing",Lean.SUSPICIOUS)),Verdict.LIKELY_ATTACK,"Wrong.")
        val finding=VerdictRuleSet(emptyList(),listOf(rule)).evaluate(emptyList())
        assertEquals(Verdict.WATCH,finding.verdict)
        assertTrue(finding.assessmentLabel.contains("INSUFFICIENT"))
    }
    @Test fun unknownRuleInputsAndUnreachableRulesAreRejected() {
        val rules=VerdictRuleSet(emptyList(),listOf(VerdictRule(Cond.Always,Verdict.WATCH,"First."),
            VerdictRule(Cond.MetricAtLeast("missing","eapol_frames",1),Verdict.WATCH,"Second.")))
        val w=Workflow("Invalid rules",1,listOf(Step("source","capture_source"),Step("timeline","target_behavior"),
            Step("report","deauth_verdict",mapOf("rules" to rules.toJson()))),
            listOf(Edge("source","timeline","frames",DataType.FRAMES),Edge("timeline","report","features",DataType.FEATURE)))
        val issues=WorkflowValidator.validate(w).joinToString()
        assertTrue(issues.contains("missing"));assertTrue(issues.contains("never run"))
    }
    @Test fun networksAndEapolAreNotCollapsedIntoIdenticalFrameCounts() {
        val base=AnalyticFrame(0,0,AnalyticFrame.DATA,"00:11:22:33:44:55","02:11:22:33:44:66","00:11:22:33:44:55")
        assertEquals(3,FrameCount.groups(listOf(base,base.copy(index=1,eapol=true),base.copy(index=2,bssid="00:11:22:33:44:77"))).size)
    }
}
