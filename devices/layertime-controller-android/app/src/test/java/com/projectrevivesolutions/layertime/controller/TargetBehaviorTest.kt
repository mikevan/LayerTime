package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.flow.*
import org.junit.Assert.*
import org.junit.Test

class TargetBehaviorTest {
    private val ap = "00:11:22:33:44:55"
    private val client = "02:11:22:33:44:66"
    private fun f(i: Int, ms: Long, sa: String = ap, da: String = client, network: String? = ap, eapol: Boolean = false) =
        AnalyticFrame(i, ms * 1000000, if(eapol) AnalyticFrame.DATA else AnalyticFrame.DEAUTH, sa, da, network, eapol=eapol)
    @Test fun correlatesBothDirectionsAndSortsEvidence() {
        val r = TargetBehavior.analyze(listOf(f(3, 200, client, ap, eapol=true), f(1, 0), f(2, 100, eapol=true))).single()
        assertEquals(client, r.target)
        assertEquals(listOf(2, 3), r.followOnFrames)
    }
    @Test fun excludesOtherClientsNetworksEarlierAndLateFrames() {
        val r = TargetBehavior.analyze(listOf(f(1,1000), f(2,0,eapol=true),
            f(3,1001,da="02:11:22:33:44:88",eapol=true),
            f(4,1002,network="00:11:22:33:44:99",eapol=true), f(5,31001,eapol=true))).single()
        assertTrue(r.followOnFrames.isEmpty())
        assertTrue(r.significance.contains("does not establish safety"))
    }
    @Test fun broadcastAndMissingNetworkDoNotAcquireClientEvidence() {
        val reports = TargetBehavior.analyze(listOf(f(1,0,da=AnalyticFrame.BROADCAST),f(2,1,network=null),f(3,2,eapol=true)))
        assertEquals(2,reports.size)
        assertTrue(reports.all { it.target == null && it.followOnFrames.isEmpty() })
    }
    @Test fun clientOriginatedDisconnectAndCaseNormalization() {
        val r = TargetBehavior.analyze(listOf(f(1,0,client.uppercase(),ap.uppercase()),f(2,1,eapol=true))).single()
        assertEquals(listOf(2),r.followOnFrames)
    }
    @Test fun includesWindowBoundaryButNotSimultaneousFrames() {
        val r = TargetBehavior.analyze(listOf(f(1,0),f(2,0,eapol=true),f(3,30000,eapol=true))).single()
        assertEquals(listOf(3),r.followOnFrames)
    }
    @Test fun timelineIsAnIndependentBuilderTool() {
        DeauthSteps.register()
        assertEquals("attack_timeline", StepRegistry.spec("target_behavior")!!.tool)
        assertTrue(StepRegistry.PALETTE.contains("target_behavior"))
        val feature = Engine.runner("target_behavior")!!.run(
            mapOf("frames" to listOf(listOf(f(1,0), f(2,1,eapol=true)))),
            mapOf("follow_on_ms" to 30000L),
            RunContext(DetectionEvent("deauth",ap,null,null,0), FrameSource { emptyList() })) as Feature
        assertEquals(Lean.SUSPICIOUS,feature.lean)
        assertEquals(listOf(1,2),feature.frames)
    }
    @Test fun emptyCaptureHasNoReports() { assertTrue(TargetBehavior.analyze(emptyList()).isEmpty()) }
}
