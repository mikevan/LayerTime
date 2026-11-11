package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.ai.*
import com.projectrevivesolutions.layertime.controller.flow.*
import org.junit.Assert.*
import org.junit.Test
import java.io.File

/** Export only fixture-derived seeds, with explicit family splits and label provenance. */
class AiTrainingSeedTest {
    @Test fun seedTargetsHaveValidEvidenceAndPreserveFamilyIsolation() {
        val splits=mapOf(
            "single_housekeeping_deauth" to "train", "legitimate_burst_inactivity" to "train",
            "broadcast_flood_reason7" to "train", "targeted_flood" to "train",
            "flood_on_protected_network" to "validation", "flood_then_evil_twin" to "test",
            "flood_without_beacons" to "test")
        val records=arrayListOf<Map<String,Any?>>()
        for(scenario in Fixtures.all()) {
            val evidence=CaptureEvidence.index("synthetic-${scenario.name}",scenario.frames,
                EvidenceCoverage("synthetic-${scenario.name}",scenario.frames.size,scenario.frames.size,training=true))
            val review=CaptureReviewer.review(evidence)
            for(page in evidence.pages(32)) {
                val request=AiReviewTasks.request("Review the supplied recording for disconnect behavior that deserves attention.",evidence,page)
                val ids=page.frames.map {it.index}.toSet()
                val issues=if(scenario.expected==Verdict.NOT_WORRISOME)emptyList() else review.issues.filter { it.frameIds.any(ids::contains) }
                val target=FlowJson.write(linkedMapOf("schemaVersion" to 1L,"issues" to issues.take(32).map {issue->
                    linkedMapOf("headline" to issue.headline,"explanation" to issue.explanation,
                        "frameIds" to issue.frameIds.take(256).map(Int::toLong))
                }))
                val decoded=AiReviewTasks.decode(target,evidence,page)
                assertTrue(decoded.flatMap {it.frameIds}.all {it in evidence.evidenceIds})
                records+=linkedMapOf("id" to "${scenario.name}-page-${page.number}","family" to scenario.name,
                    "split" to splits.getValue(scenario.name),"task" to "REVIEW_CAPTURE",
                    "sourceId" to "layertime-fixtures-v1","labelStatus" to "fixture-derived-not-field-validated",
                    "outputSchema" to request.outputSchema,
                    "evidenceIds" to evidence.evidenceIds.sorted().map(Int::toLong),
                    "messages" to request.messages.map {mapOf("role" to it.role.name.lowercase(),"content" to it.content)} +
                        listOf(mapOf("role" to "assistant","content" to target)))
            }
        }
        assertEquals(setOf("train","validation","test"),records.map {it["split"]}.toSet())
        assertTrue(records.size>10)
        val dir=System.getenv("LAYERTIME_AI_SEED_EXPORT")?.let(::File)
        if(dir!=null) {
            dir.mkdirs()
            File(dir,"seeds.jsonl").writeText(records.joinToString("\n",postfix="\n") {FlowJson.write(it)})
        }
    }
}
