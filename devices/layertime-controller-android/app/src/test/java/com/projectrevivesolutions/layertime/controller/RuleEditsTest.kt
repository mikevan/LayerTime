package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.flow.*
import org.junit.Assert.*
import org.junit.Test

/** Build and exchange the report using the same immutable edits as the phone controls. */
class RuleEditsTest {
    private val checks=listOf(Step("packets","deauth_rate"),Step("behavior","target_behavior"))
    @Test fun newConditionalRuleGoesBeforeFallback() {
        val set=RuleEdits.add(RuleEdits.initial(),RuleEdits.defaultCondition(checks))
        assertEquals(Cond.MetricAtLeast("behavior","linked_episodes",1),set.rules.first().cond)
        assertEquals(Cond.Always,set.rules.last().cond)
    }
    @Test fun moveAndRemovePreserveOtherRules() {
        val set=RuleEdits.add(RuleEdits.initial(),Cond.CheckLeans("packets",Lean.SUSPICIOUS))
        assertEquals(set.rules.reversed(),RuleEdits.move(set,0,1).rules)
        assertEquals(set,RuleEdits.move(set,0,-1))
        assertEquals(listOf(set.rules.last()),RuleEdits.remove(set,0).rules)
    }
    @Test fun selectingCountedChecksDoesNotDuplicateThem() {
        val set=RuleEdits.count(RuleEdits.count(RuleEdits.initial(),"packets",true),"packets",true)
        assertEquals(listOf("packets"),set.counted)
        assertTrue(RuleEdits.count(set,"packets",false).counted.isEmpty())
    }
    @Test fun visualRuleRoundTripPreservesNestedLogicHeadingAndSentence() {
        var set=RuleEdits.add(RuleEdits.initial(),Cond.All(listOf(Cond.MetricAtLeast("behavior","linked_episodes",1),
            Cond.Any(listOf(Cond.CheckLeans("packets",Lean.SUSPICIOUS),Cond.Not(Cond.CheckLeans("packets",Lean.UNKNOWN)))))))
        val first=set.rules.first().copy(verdict=Verdict.LIKELY_ATTACK,label="My community posture",summary="{behavior.eapol_frames} EAPOL frames followed.")
        set=RuleEdits.replace(set,0,first)
        assertEquals(set,VerdictRuleSet.parse(set.toJson()))
        val result=set.evaluate(listOf(Feature("packets","Packet rate","burst",Lean.SUSPICIOUS,"Burst."),
            Feature("behavior","Timeline","linked",Lean.SUSPICIOUS,"Linked.",metrics=mapOf("linked_episodes" to 1L,"eapol_frames" to 2L))))
        assertEquals("My community posture",result.assessmentLabel)
        assertEquals("2 EAPOL frames followed.",result.summary)
    }
    @Test fun authoredWorkflowRunsAfterExportAndImport() {
        DeauthSteps.register()
        val rules=RuleEdits.add(RuleEdits.initial(),RuleEdits.defaultCondition(checks))
        val flow=Workflow("Phone-authored posture",1,listOf(Step("recording","capture_source"))+checks+
            Step("report","deauth_verdict",mapOf("rules" to rules.toJson())),listOf(
                Edge("recording","packets","frames",DataType.FRAMES),Edge("recording","behavior","frames",DataType.FRAMES),
                Edge("packets","report","features",DataType.FEATURE),Edge("behavior","report","features",DataType.FEATURE)))
        val imported=WorkflowExchange.read(DraftEdits.write(DraftEdits.open(flow,null,false)).byteInputStream()).workflow
        assertTrue(WorkflowValidator.validate(imported).isEmpty())
        val ap="00:11:22:33:44:55";val client="02:11:22:33:44:66"
        val disconnect=AnalyticFrame(0,0,AnalyticFrame.DEAUTH,ap,client,ap)
        fun run(second:AnalyticFrame)=Engine.run(imported,RunContext(DetectionEvent("deauth",ap,null,null,0),FrameSource {listOf(disconnect,second)})).finding!!
        val eapol=AnalyticFrame(1,1000000,AnalyticFrame.DATA,client,ap,ap,eapol=true)
        assertEquals("Describe what these observations mean.",run(eapol).summary)
        assertEquals("Review the available observations and their supporting evidence. 1 check could not run.",run(eapol.copy(sa="02:11:22:33:44:88")).summary)
    }
    @Test fun frameCountConditionRequiresAnActualFrameConnectionEvenWhenNested() {
        val rules=RuleEdits.add(RuleEdits.initial(),Cond.Not(Cond.All(listOf(Cond.GroupAtLeast(5)))))
        val w=Workflow("Missing frames",1,listOf(Step("s","capture_source"),Step("t","target_behavior"),
            Step("report","deauth_verdict",mapOf("rules" to rules.toJson()))),listOf(
                Edge("s","t","frames",DataType.FRAMES),Edge("t","report","features",DataType.FEATURE)))
        assertTrue(WorkflowValidator.validate(w).any {it.port=="frames" && it.step=="report"})
    }
}
