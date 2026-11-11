package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.ai.*
import com.projectrevivesolutions.layertime.controller.flow.*
import org.junit.Assert.*
import org.junit.Test
import java.io.File

/** Linkage teaching candidates use the actual whole-capture Timeline, not a burst-only surrogate. */
class AiCorrelationTrainingSeedTest {
    private val ap = "02:00:00:00:01:01"
    private val other = "02:00:00:00:02:02"
    private val client = "02:00:00:00:03:03"
    private val second = "02:00:00:00:04:04"
    private val origin = 1_700_000_000_000_000_000L
    private fun frame(id: Int, ms: Long, kind: Int = AnalyticFrame.DEAUTH, network: String? = ap,
                      destination: String = client, source: String = ap) =
        AnalyticFrame(id, origin + ms * 1_000_000L, kind, source, destination, network,
            reasonCode = if (kind == AnalyticFrame.DEAUTH) 7 else null, eapol = kind == AnalyticFrame.DATA)
    private data class Case(val name: String, val frames: List<AnalyticFrame>, val episodes: Int,
                            val links: Int, val match: NetworkMatch?, val gap: Long = 3000)

    @Test fun wholeRecordingLinksAndCrossPageReferencesAreExportedWithoutInventingOutcomes() {
        val twoEpisodes = listOf(frame(0, 0), frame(1, 1000, AnalyticFrame.DATA),
            frame(2, 10000), frame(3, 11000, AnalyticFrame.DATA, source = client, destination = ap))
        val cases = listOf(
            Case("same_pair_reverse_exchange", listOf(frame(0,0),frame(1,500,AnalyticFrame.DATA,source=client,destination=ap)),1,1,NetworkMatch.SAME_DEVICE_AND_ACCESS_POINT),
            Case("different_network", listOf(frame(0,0),frame(1,500,AnalyticFrame.DATA,network=other,source=other)),1,0,NetworkMatch.DIFFERENT_ACCESS_POINTS),
            Case("different_client", listOf(frame(0,0),frame(1,500,AnalyticFrame.DATA,destination=second)),1,0,NetworkMatch.NO_LINKED_EAPOL),
            Case("exchange_before_request", listOf(frame(0,0,AnalyticFrame.DATA),frame(1,500)),1,0,NetworkMatch.SAME_DEVICE_AND_ACCESS_POINT),
            Case("equal_timestamps", listOf(frame(0,0),frame(1,0,AnalyticFrame.DATA)),1,0,NetworkMatch.SAME_DEVICE_AND_ACCESS_POINT),
            Case("unresolved_network", listOf(frame(0,0,network=null),frame(1,500,AnalyticFrame.DATA)),1,0,NetworkMatch.UNKNOWN),
            Case("broadcast_target", listOf(frame(0,0,destination=AnalyticFrame.BROADCAST),frame(1,500,AnalyticFrame.DATA)),1,0,NetworkMatch.SAME_ACCESS_POINT_TARGET_UNKNOWN),
            Case("separate_episodes",twoEpisodes,2,2,NetworkMatch.SAME_DEVICE_AND_ACCESS_POINT),
            Case("merged_episode",twoEpisodes,1,1,NetworkMatch.SAME_DEVICE_AND_ACCESS_POINT,gap=20000),
            Case("exchange_between_requests",listOf(frame(0,0),frame(1,100),frame(2,150,AnalyticFrame.DATA),frame(3,200)),1,1,NetworkMatch.SAME_DEVICE_AND_ACCESS_POINT),
            Case("invalid_exchange_clock",listOf(frame(0,0),frame(1,500,AnalyticFrame.DATA).copy(timestampValid=false)),1,0,NetworkMatch.NO_LINKED_EAPOL),
            Case("malformed_request",listOf(frame(0,0).copy(malformed=true),frame(1,500,AnalyticFrame.DATA)),0,0,null),
            Case("shared_name_different_access_points",listOf(
                frame(0,0,AnalyticFrame.BEACON).copy(ssid="CampNet"),
                frame(1,10,AnalyticFrame.BEACON,network=other,source=other).copy(ssid="CampNet"),
                frame(2,100),frame(3,500,AnalyticFrame.DATA,network=other,source=other)),1,0,NetworkMatch.DIFFERENT_ACCESS_POINTS),
            Case("multiple_names_one_access_point",listOf(
                frame(0,0,AnalyticFrame.BEACON).copy(ssid="CampNet"),
                frame(1,10,AnalyticFrame.BEACON).copy(ssid="CampNet-Guest"),
                frame(2,100),frame(3,500,AnalyticFrame.DATA)),1,1,NetworkMatch.SAME_DEVICE_AND_ACCESS_POINT),
        )
        val records = arrayListOf<Map<String,Any?>>()
        for (case in cases) {
            val capture = "synthetic-correlation-${case.name}"
            val coverage = EvidenceCoverage(capture,case.frames.size,case.frames.size,training=true,
                malformedFrames=case.frames.count {it.malformed},untimedFrames=case.frames.count {!it.timestampValid})
            val evidence = CaptureEvidence.index(capture,case.frames,coverage,episodeGapMs=case.gap)
            assertEquals(case.name,case.episodes,evidence.episodes.size)
            assertEquals(case.name,case.links,evidence.episodes.count {it.followOnFrames.isNotEmpty()})
            case.match?.let { expected -> evidence.episodes.forEach {assertEquals(case.name,expected,NetworkComparisons.read(it).match)} }
            val prompt = "Explain the device and Wi-Fi access-point match and event order for each supplied disconnect episode. " +
                "This is a Timeline linkage review, not a six-frame burst-only analytic. Include uncertain and unlinked disconnect observations. " +
                "Use plain language and distinguish requests and exchanges from confirmed outcomes."
            for (page in evidence.pages(2)) {
                val request = AiReviewTasks.request(prompt,evidence,page)
                val pageIds = page.frames.map {it.index}.toSet()
                val episodes = evidence.episodes.filter {r -> r.comparisonEvidence.any(pageIds::contains)}
                val issues = episodes.map {report ->
                    val comparison=NetworkComparisons.read(report)
                    val headline=when {
                        case.name=="invalid_exchange_clock" -> "A disconnect request is visible; the security exchange's timing is unknown."
                        case.name=="different_client" -> "The security exchange involved a different device."
                        report.followOnFrames.isNotEmpty() -> "Disconnect requests and later security messages involved the same device and access point."
                        comparison.match==NetworkMatch.DIFFERENT_ACCESS_POINTS -> "The security exchange used a different access point."
                        comparison.match==NetworkMatch.SAME_ACCESS_POINT_TARGET_UNKNOWN -> "The request was broadcast; one reconnecting device cannot be established."
                        comparison.order.contains("BEFORE") -> "The security exchange happened before the disconnect request."
                        comparison.order.contains("share a timestamp") -> "The event order is unknown because the timestamps are equal."
                        comparison.match==NetworkMatch.UNKNOWN -> "The device or access-point match is unresolved."
                        else -> "A disconnect request is visible; the later outcome is unknown."
                    }
                    val disconnectCount=report.disconnectFrames.size
                    val exchangeCount=report.followOnFrames.size
                    val countText=if(disconnectCount==1) "One disconnect request was observed. "
                        else "$disconnectCount disconnect requests were observed. "
                    val contextText=when(case.name) {
                        "invalid_exchange_clock" -> "A security-exchange frame for device $client and access point $ap is visible, but its timestamp is invalid. " +
                            "It cannot establish whether the exchange followed the disconnect request. "
                        "different_client" -> "The disconnect request identifies device $client, while the security exchange identifies device $second. " +
                            "Both use access point $ap, but the devices do not match. "
                        "unresolved_network" -> "The disconnect request has no usable access-point identifier. A later-timestamped security-exchange frame is visible, " +
                            "but its access point cannot be matched to the request. "
                        else -> comparison.explanation + " " +
                            (if(exchangeCount>0) (if(exchangeCount==1) "One security-exchange frame" else "$exchangeCount security-exchange frames") +
                                " for the same pair followed at least one disconnect request. " else comparison.order + " ")
                    }
                    val nameText=when(case.name) {
                        "shared_name_different_access_points" -> "Access points $ap and $other both advertised the name CampNet. A shared name does not make their BSSIDs identical or prove impersonation. "
                        "multiple_names_one_access_point" -> "Access point $ap was observed advertising both CampNet and CampNet-Guest. These observations do not establish a unique current name or ownership. "
                        else -> ""
                    }
                    val additionalRefs=if(case.name in setOf("invalid_exchange_clock","different_client","unresolved_network"))
                        page.frames.filter {it.eapol && !it.malformed}.map {it.index} else emptyList()
                    val nameRefs=AiEvidenceWire.networkContext(evidence,page).flatMap {it.frameIds}
                    val explanation=countText + contextText + nameText +
                        "These observations do not prove the device disconnected, completed a reconnect, or lost a password. " +
                        coverage.limitations.joinToString(" ")
                    linkedMapOf("headline" to headline,"explanation" to explanation,
                        "frameIds" to (report.comparisonEvidence + additionalRefs + nameRefs).distinct().map(Int::toLong))
                }
                val target=FlowJson.write(linkedMapOf("schemaVersion" to 1L,"issues" to issues))
                val decoded=AiReviewTasks.decode(target,evidence,page)
                assertTrue(decoded.flatMap {it.frameIds}.all {it in evidence.evidenceIds})
                if(case.name=="invalid_exchange_clock") {
                    assertTrue(target.contains("timestamp is invalid"))
                    assertTrue(decoded.flatMap {it.frameIds}.contains(1))
                }
                assertFalse(target.contains("request(s)"))
                records+=linkedMapOf("id" to "correlation-v3-${case.name}-page-${page.number}",
                    "family" to case.name,"task" to "REVIEW_CAPTURE","sourceId" to "layertime-correlation-candidates-v3",
                    "labelStatus" to "authored-linkage-candidate-whole-capture-engine-verified","trainingApproved" to false,
                    "evidenceIds" to evidence.evidenceIds.sorted().map(Int::toLong),"outputSchema" to request.outputSchema,
                    "expectedFacts" to mapOf("captureEpisodes" to case.episodes.toLong(),"linkedEpisodes" to case.links.toLong(),
                        "networkMatch" to case.match?.name),
                    "messages" to request.messages.map {mapOf("role" to it.role.name.lowercase(),"content" to it.content)} +
                        listOf(mapOf("role" to "assistant","content" to target)))
            }
        }
        assertEquals(14,records.map {it["family"]}.distinct().size)
        System.getenv("LAYERTIME_AI_CORRELATION_EXPORT")?.let { destination ->
            val dir=File(destination);dir.mkdirs()
            File(dir,"candidates.jsonl").writeText(records.joinToString("\n",postfix="\n") {FlowJson.write(it)})
        }
    }
}
