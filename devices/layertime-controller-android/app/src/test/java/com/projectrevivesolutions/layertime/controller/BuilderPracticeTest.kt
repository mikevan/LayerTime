package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.flow.*
import org.junit.Assert.*
import org.junit.Test

class BuilderPracticeTest {
    private fun run(threshold:Long):Finding {
        DeauthSteps.register()
        val frames=BuilderPractice.fixture().built
        val base=BuilderPractice.workflow()
        val w=base.copy(steps=base.steps.map {if(it.type=="deauth_verdict")it.copy(settings=mapOf("rules" to BuilderPractice.expectedRules(threshold).toJson())) else it})
        assertTrue(WorkflowValidator.validate(w).isEmpty())
        return Engine.run(w,RunContext(DetectionEvent("deauth",BuilderPractice.AP,6,-50,frames.first().timeNanos),FrameSource { frames },
            EvidenceCoverage("practice",6,6,training=true))).finding!!
    }
    @Test fun referenceProducesExactCountsAndExcludesOtherClient() {
        val f=run(2)
        assertEquals(BuilderPractice.HEADING,f.assessmentLabel)
        assertEquals("2 disconnect episodes, 2 linked episodes, 2 related EAPOL frames.",f.summary)
        val feature=f.features.single {it.checkType=="target_behavior"}
        assertEquals(2L,feature.metrics["linked_episodes"])
        assertEquals(2L,feature.metrics["eapol_frames"])
        assertEquals(listOf(0,1,2),feature.targetReports[0].evidence)
        assertEquals(listOf(4,5),feature.targetReports[1].evidence)
        assertFalse(feature.frames.contains(3))
        assertTrue(f.coverage!!.sentence.startsWith("TRAINING DATA"))
    }
    @Test fun changingThresholdChangesAndRestoresTheHeading() {
        assertEquals(BuilderPractice.HEADING,run(2).assessmentLabel)
        assertEquals(BuilderPractice.FALLBACK,run(3).assessmentLabel)
        assertEquals(BuilderPractice.HEADING,run(2).assessmentLabel)
    }
    @Test fun teachingPcapHasSixPacketsAndStructuredRsnEapolDescriptors() {
        val bytes=BuilderPractice.fixture().pcap()
        fun u8(i:Int)=bytes[i].toInt() and 255
        fun le32(i:Int)=u8(i) or (u8(i+1) shl 8) or (u8(i+2) shl 16) or (u8(i+3) shl 24)
        var position=24;var records=0;var keys=0
        while(position<bytes.size) {
            val length=le32(position+8);val start=position+16
            val radio=u8(start+2) or (u8(start+3) shl 8)
            val llc=start+radio+24
            if(u8(start+radio)==0x08 && u8(llc+6)==0x88 && u8(llc+7)==0x8e) {
                val eapol=llc+8
                assertEquals(2,u8(eapol));assertEquals(3,u8(eapol+1))
                assertEquals(95,u8(eapol+3));assertEquals(2,u8(eapol+4))
                keys++
            }
            records++;position=start+length
        }
        assertEquals(6,records);assertEquals(3,keys);assertEquals(bytes.size,position)
    }
    @Test fun referenceRoundTripsLikeAnOperatorWorkflow() {
        val d=DraftEdits.copyOf(DraftEdits.open(BuilderPractice.workflow(),null,true))
        val restored=WorkflowExchange.read(DraftEdits.write(d).byteInputStream())
        assertEquals(d,restored)
    }
}
