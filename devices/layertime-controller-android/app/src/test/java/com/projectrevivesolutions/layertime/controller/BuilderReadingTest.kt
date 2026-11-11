package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.flow.*
import org.junit.Assert.*
import org.junit.Test

class BuilderReadingTest {
    @Test fun tapToAddWiresOneSourceAndOneReport() {
        val added=GuidedBuilder.add(GuidedBuilder.newAnalysis(),"target_behavior")!!
        assertEquals(2,added.first.workflow.edges.size)
        assertTrue(WorkflowValidator.validate(added.first.workflow).isEmpty())
    }
    @Test fun assigningTappedWiresharkPlaceholderConnectsIt() {
        val added=GuidedBuilder.add(GuidedBuilder.newAnalysis(),StepRegistry.WIRESHARK_PLACEHOLDER)!!
        val assigned=GuidedBuilder.attach(DraftEdits.assign(added.first,added.second,"deauth_rate"),added.second)
        assertEquals(2,assigned.workflow.edges.size)
        assertTrue(WorkflowValidator.validate(assigned.workflow).isEmpty())
    }
    @Test fun readingsNameDeauthCountTargetAndExcludeDistractor() {
        val reports=TargetBehavior.analyze(BuilderPractice.fixture().built)
        val reading=ReportReading.devices(reports).single()
        assertEquals(listOf(0,1,4),reading.deauth)
        assertEquals(listOf(2,5),reading.setup)
        assertTrue(reading.observation.contains(BuilderPractice.CLIENT))
        assertTrue(reading.observation.contains("target of 3 deauthentication (deauth) packets"))
        assertFalse(reading.observation.contains(BuilderPractice.OTHER))
        assertTrue(reading.significance.contains("weak password"))
        assertTrue(reading.action.contains("Increase your threat posture"))
    }
    @Test fun clientOriginatedDisconnectIsNotReportedAsTargetedAtClient() {
        val f=AnalyticFrame(0,0,AnalyticFrame.DEAUTH,BuilderPractice.CLIENT,BuilderPractice.AP,BuilderPractice.AP)
        val reading=ReportReading.devices(TargetBehavior.analyze(listOf(f))).single()
        assertTrue(reading.directed.isEmpty())
        assertFalse(reading.observation.contains("was the target"))
        assertTrue(reading.observation.contains("sent by this device"))
    }
    @Test fun understandableQuestionRunsTheSameEvidencePipeline() {
        DeauthSteps.register()
        val w=GuidedBuilder.questionAnalysis()
        assertTrue(WorkflowValidator.validate(w).isEmpty())
        val f=Engine.run(w,RunContext(DetectionEvent("deauth",BuilderPractice.AP,6,-50,0),FrameSource {BuilderPractice.fixture().built})).finding!!
        assertEquals("DISCONNECT REQUESTS FOLLOWED BY SETUP",f.assessmentLabel)
        assertEquals(3,ReportReading.devices(f.features.flatMap {it.targetReports}).single().deauth.size)
    }
}
