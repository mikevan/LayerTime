package com.projectrevivesolutions.layertime.controller.flow

enum class NetworkMatch { SAME_DEVICE_AND_ACCESS_POINT, SAME_ACCESS_POINT_TARGET_UNKNOWN, DIFFERENT_ACCESS_POINTS, UNKNOWN, NO_LINKED_EAPOL }
data class NetworkComparison(val match:NetworkMatch,val headline:String,val explanation:String,val evidence:List<Int>,
    val eapolAccessPoints:List<String> = emptyList(),val order:String="")

/** BSSID comparisons are observations, not attribution or proof of a physical transmitter's identity. */
object NetworkComparisons {
    fun read(report:TargetReport):NetworkComparison=read(listOf(report))
    fun read(reports:List<TargetReport>):NetworkComparison {
        val first=reports.firstOrNull()
        val network=first?.network
        val target=first?.target
        val linked=reports.flatMap {it.followOnFrames}.distinct()
        val candidates=reports.flatMap {it.eapolNetworkCandidates}
        val different=candidates.mapNotNull {it.network}.filter {it!=network}.distinct().sorted()
        val evidence=reports.flatMap {it.comparisonEvidence}.distinct().sorted()
        val same=candidates.filter {it.network==network && network!=null}
        val before=same.filter {it.timing==EapolTiming.BEFORE}.flatMap {it.frames}.distinct()
        val after=(same.filter {it.timing==EapolTiming.AFTER}.flatMap {it.frames}+linked).distinct()
        val simultaneous=same.any {it.timing==EapolTiming.SAME_TIMESTAMP}
        val order=when {
            before.isNotEmpty() && after.isNotEmpty()->"${before.size} EAPOL before; ${after.size} after disconnect requests."
            before.isNotEmpty()->"${before.size} EAPOL came BEFORE the disconnect. No later same-pair reconnection was established."
            after.isNotEmpty()->"${after.size} EAPOL came AFTER the disconnect. Timing alone does not establish cause."
            simultaneous->"EAPOL and disconnect share a timestamp; their order is not established."
            candidates.isNotEmpty()->"EAPOL was observed for a different or unresolved access point; no same-pair sequence is claimed."
            else->"No comparable EAPOL was observed in the selected comparison window."
        }
        val result=when {
            (linked.isNotEmpty() || same.isNotEmpty()) && network!=null && target!=null -> NetworkComparison(NetworkMatch.SAME_DEVICE_AND_ACCESS_POINT,
                "MATCH: same device and Wi-Fi access point",
                "Disconnect and EAPOL frames both identify device $target and access point $network (BSSID). "+
                    if(different.isNotEmpty())"Other EAPOL traffic for this device used ${different.joinToString()}; it is kept separate." else "The same network name alone was not used to make this match.",evidence)
            network==null -> NetworkComparison(NetworkMatch.UNKNOWN,"Network match not established",
                "The disconnect frames have no usable access-point identifier (BSSID), so their network cannot be matched to EAPOL traffic.",evidence)
            target==null && candidates.any {it.network==network} -> NetworkComparison(NetworkMatch.SAME_ACCESS_POINT_TARGET_UNKNOWN,
                "Same access point; affected device not established",
                "Disconnect and EAPOL traffic identify access point $network. The disconnect was broadcast or its target could not be resolved; a same-device reconnection has not been established.",evidence)
            target!=null && different.isNotEmpty() -> NetworkComparison(NetworkMatch.DIFFERENT_ACCESS_POINTS,
                "EAPOL was seen on a different Wi-Fi access point",
                "Disconnect access point: $network. EAPOL for device $target used ${different.joinToString()}. "+
                    "These exchanges are not counted as a same-access-point reconnection. Different access points may belong to one network during roaming.",evidence)
            target!=null && candidates.any {it.network==null} -> NetworkComparison(NetworkMatch.UNKNOWN,"Network match not established",
                "EAPOL involved device $target, but its access-point identifier was missing. It cannot establish a match to $network.",evidence)
            target==null -> NetworkComparison(NetworkMatch.UNKNOWN,"Device match not established",
                "Disconnect access point: $network. A particular affected device could not be identified, so no same-device EAPOL sequence is claimed.",evidence)
            else -> NetworkComparison(NetworkMatch.NO_LINKED_EAPOL,"No later EAPOL linked to this device and access point",
                "No later EAPOL was linked to device $target on access point $network within the selected window. Missing traffic does not establish safety.",evidence)
        }
        return result.copy(eapolAccessPoints=(candidates.mapNotNull {it.network}+if(linked.isNotEmpty())listOfNotNull(network)else emptyList()).distinct().sorted(),order=order)
    }
}
