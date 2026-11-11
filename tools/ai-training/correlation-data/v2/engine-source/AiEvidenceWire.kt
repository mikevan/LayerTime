package com.projectrevivesolutions.layertime.controller.ai

import com.projectrevivesolutions.layertime.controller.flow.*

/** Recording fields are JSON data, never instructions. Preserve links back to exact frame indexes. */
object AiEvidenceWire {
    /** One cited observation per relevant BSSID/name pair; identical names never merge access points. */
    fun networkContext(evidence:CaptureEvidence,page:CaptureEvidence.Page):List<CaptureEvidence.NamedNetwork> {
        evidence.validatePage(page)
        val pageIds=page.frames.map {it.index}.toSet()
        val related=evidence.episodes.filter {r->r.comparisonEvidence.any(pageIds::contains)}
        val networks=(page.frames.mapNotNull {TargetBehavior.address(it.bssid)} +
            related.mapNotNull {it.network} + related.flatMap {it.eapolNetworkCandidates.mapNotNull {candidate->candidate.network}}).toSet()
        return evidence.networkNames(networks).sortedWith(compareBy({it.bssid},{it.ssid}))
    }

    fun page(evidence: CaptureEvidence, page: CaptureEvidence.Page): String {
        evidence.validatePage(page)
        return FlowJson.write(linkedMapOf(
        "schemaVersion" to 1L, "captureId" to evidence.captureId,
        "page" to page.number.toLong(), "pages" to page.total.toLong(),
        "availableFrames" to evidence.frames.size.toLong(), "limitations" to evidence.coverage.limitations,
        "training" to evidence.coverage.training,
        "networkContext" to networkContext(evidence,page).map {n->linkedMapOf(
            "bssid" to n.bssid,"ssid" to n.ssid,"frameIds" to n.frameIds.map(Int::toLong))},
        "observations" to page.frames.map { f -> linkedMapOf(
            "frameId" to f.index.toLong(), "timeNanos" to f.timeNanos, "timestampValid" to f.timestampValid,
            "malformed" to f.malformed, "typeSubtype" to f.typeSubtype.toLong(),
            "source" to f.sa, "destination" to f.da, "network" to f.bssid,
            "eapol" to f.eapol, "reasonCode" to f.reasonCode?.toLong(),
            "sequence" to f.seq?.toLong(), "retry" to f.retry, "rssi" to f.rssi?.toLong(),
            "channel" to f.channel?.toLong(), "ssid" to f.ssid,
            "managementProtectionCapable" to f.mfpCapable, "managementProtectionRequired" to f.mfpRequired,
        ) },
        // Include episodes touching this page, with their cross-page evidence preserved.
        "relatedEpisodes" to evidence.episodes.filter { r -> (r.evidence+r.comparisonEvidence).any { id -> page.frames.any { it.index == id } } }.map { r ->
            linkedMapOf("target" to r.target, "network" to r.network,
                "disconnectFrames" to r.disconnectFrames.map(Int::toLong),
                "eapolFrames" to r.followOnFrames.map(Int::toLong),
                "networkComparison" to r.eapolNetworkCandidates.map {candidate->mapOf("bssid" to candidate.network,"timing" to candidate.timing.name,"frames" to candidate.frames.map(Int::toLong))},
                "followOnMs" to evidence.followOnMs, "episodeGapMs" to evidence.episodeGapMs)
        },
        ))
    }
}
