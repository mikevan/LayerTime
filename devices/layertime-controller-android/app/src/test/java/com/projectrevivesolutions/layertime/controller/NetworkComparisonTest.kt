package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.flow.*
import org.junit.Assert.*
import org.junit.Test

class NetworkComparisonTest {
    private val ap="02:00:00:00:00:01"
    private val other="02:00:00:00:00:03"
    private val client="02:00:00:00:00:02"
    private fun deauth(target:String=client,bssid:String?=ap)=AnalyticFrame(0,1_000_000_000,AnalyticFrame.DEAUTH,ap,target,bssid)
    private fun eapol(source:String=ap,target:String=client,bssid:String?=ap)=AnalyticFrame(1,2_000_000_000,AnalyticFrame.DATA,source,target,bssid,eapol=true)
    @Test fun matchingDeviceAndBssidIsExplicit() {
        val r=TargetBehavior.analyze(listOf(deauth(),eapol())).single()
        assertEquals(NetworkMatch.SAME_DEVICE_AND_ACCESS_POINT,NetworkComparisons.read(r).match)
        assertEquals(listOf(1),r.followOnFrames)
    }
    @Test fun sameDeviceOnOtherBssidDoesNotBecomeLinkedReconnection() {
        val r=TargetBehavior.analyze(listOf(deauth(),eapol(source=other,bssid=other))).single()
        assertEquals(NetworkMatch.DIFFERENT_ACCESS_POINTS,NetworkComparisons.read(r).match)
        assertTrue(r.followOnFrames.isEmpty())
        assertEquals(listOf(other),NetworkComparisons.read(r).eapolAccessPoints)
        assertEquals(listOf(0,1),r.comparisonEvidence)
    }
    @Test fun broadcastCanMatchNetworkWithoutClaimingAnAffectedClient() {
        val r=TargetBehavior.analyze(listOf(deauth(AnalyticFrame.BROADCAST),eapol())).single()
        assertEquals(NetworkMatch.SAME_ACCESS_POINT_TARGET_UNKNOWN,NetworkComparisons.read(r).match)
        assertTrue(r.followOnFrames.isEmpty())
    }
    @Test fun missingEapolNetworkIdentifierRemainsUnknown() {
        val r=TargetBehavior.analyze(listOf(deauth(),eapol(bssid=null))).single()
        assertEquals(NetworkMatch.UNKNOWN,NetworkComparisons.read(r).match)
        assertTrue(r.followOnFrames.isEmpty())
    }
    @Test fun unrelatedDeviceDoesNotSupplyNetworkComparisonEvidence() {
        val r=TargetBehavior.analyze(listOf(deauth(),eapol(target="02:00:00:00:00:04"))).single()
        assertEquals(NetworkMatch.NO_LINKED_EAPOL,NetworkComparisons.read(r).match)
        assertEquals(listOf(0),r.comparisonEvidence)
    }
    @Test fun unrelatedLateTrafficDoesNotBecomeAComparison() {
        val r=TargetBehavior.analyze(listOf(deauth(),eapol(source=other,bssid=other).copy(timeNanos=40_000_000_000))).single()
        assertEquals(NetworkMatch.NO_LINKED_EAPOL,NetworkComparisons.read(r).match)
    }
    @Test fun priorEapolCanMatchIdentityWithoutBecomingALaterReconnection() {
        val r=TargetBehavior.analyze(listOf(eapol().copy(index=0,timeNanos=1_000_000_000),deauth().copy(index=1,timeNanos=4_000_000_000))).single()
        val comparison=NetworkComparisons.read(r)
        assertEquals(NetworkMatch.SAME_DEVICE_AND_ACCESS_POINT,comparison.match)
        assertTrue(comparison.order.contains("BEFORE"))
        assertTrue(r.followOnFrames.isEmpty())
        assertEquals(listOf(0,1),comparison.evidence)
    }
    @Test fun identicalTimestampsDoNotInventAnEventOrder() {
        val r=TargetBehavior.analyze(listOf(deauth(),eapol().copy(timeNanos=1_000_000_000))).single()
        assertEquals(NetworkMatch.SAME_DEVICE_AND_ACCESS_POINT,NetworkComparisons.read(r).match)
        assertTrue(NetworkComparisons.read(r).order.contains("order is not established"))
        assertTrue(r.followOnFrames.isEmpty())
    }
}
