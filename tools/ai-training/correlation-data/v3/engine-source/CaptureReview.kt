package com.projectrevivesolutions.layertime.controller.ai

import com.projectrevivesolutions.layertime.controller.flow.*

/** One immutable recording, indexed completely before model-sized pages are constructed. */
class CaptureEvidence private constructor(
    val captureId: String,
    val frames: List<AnalyticFrame>,
    val coverage: EvidenceCoverage,
    val episodes: List<TargetReport>,
    val followOnMs: Long,
    val episodeGapMs: Long,
) {
    data class Page(val captureId: String, val number: Int, val total: Int, val frames: List<AnalyticFrame>)
    data class NamedNetwork(val bssid:String,val ssid:String,val frameIds:List<Int>)
    private val byId = frames.associateBy { it.index }
    // Index once for the complete recording; page construction must not repeatedly scan every beacon.
    private val namesByNetwork=frames.asSequence().filter {f->!f.malformed && f.timestampValid &&
        f.typeSubtype in setOf(AnalyticFrame.BEACON,AnalyticFrame.PROBE_RESP) &&
        TargetBehavior.address(f.bssid)!=null && !f.ssid.isNullOrBlank()}
        .groupBy {TargetBehavior.address(it.bssid)!! to it.ssid!!}
        .map {(key,observations)->NamedNetwork(key.first,key.second,listOf(observations.first().index))}
        .groupBy {it.bssid}
    fun networkNames(networks:Set<String>):List<NamedNetwork> = networks.flatMap {namesByNetwork[it].orEmpty()}
    val evidenceIds: Set<Int> = byId.keys
    fun validatePage(page: Page) {
        require(page.captureId == captureId && page.number in 1..page.total && page.frames.isNotEmpty()) {
            "Evidence page belongs to a different recording."
        }
        require(page.frames.map {it.index}.distinct().size==page.frames.size && page.frames.all {byId[it.index]==it}) {
            "Evidence page contains observations outside this recording."
        }
    }
    fun pages(pageSize: Int = 128): List<Page> {
        require(pageSize in 1..512)
        val chunks = frames.chunked(pageSize)
        return chunks.mapIndexed { i, fs -> Page(captureId, i + 1, chunks.size, fs) }
    }

    /** Temporal correlation runs across the entire recording, never independently within pages. */
    companion object {
        const val MAX_FRAMES = 200_000
        fun index(captureId: String, frames: List<AnalyticFrame>, coverage: EvidenceCoverage? = null,
                  followOnMs: Long = 30_000, episodeGapMs: Long = 3_000): CaptureEvidence {
            require(captureId.isNotBlank()) { "The recording needs an identity." }
            require(frames.size <= MAX_FRAMES) { "Recording exceeds the supported $MAX_FRAMES frame limit; no partial review was started." }
            require(frames.all { it.index >= 0 } && frames.map { it.index }.distinct().size == frames.size) {
                "Recording frame references must be nonnegative and unique."
            }
            require(coverage == null || coverage.captureId == captureId) { "Recording coverage belongs to a different capture." }
            require(coverage == null || (coverage.analyzedFrames == frames.size && coverage.decodedFrames >= frames.size)) {
                "Recording coverage does not match the supplied observations."
            }
            val snapshot = frames.toList()
            val supplied = coverage ?: EvidenceCoverage(captureId, snapshot.size, snapshot.size,
                malformedFrames = snapshot.count { it.malformed }, untimedFrames = snapshot.count { !it.timestampValid })
            return CaptureEvidence(captureId, snapshot, supplied,
                TargetBehavior.analyze(snapshot, followOnMs, episodeGapMs), followOnMs, episodeGapMs)
        }
    }
}

enum class ReviewLevel { REVIEW, SIGNIFICANT }
data class RecordingIssue(
    val id: String, val level: ReviewLevel, val headline: String, val explanation: String,
    val frameIds: List<Int>, val target: String?, val network: String?,
    val disconnectCount: Int, val eapolCount: Int,
    val networkComparison: NetworkComparison,
    val networkNames:List<String> = emptyList(),
)
data class RecordingReview(
    val captureId: String, val reviewedFrames: Int, val issues: List<RecordingIssue>,
    val coverage: EvidenceCoverage,
    val followOnMs: Long = 30_000, val episodeGapMs: Long = 3_000,
) {
    val headline: String get() = when {
        issues.isNotEmpty() -> "${issues.size} activity ${if (issues.size == 1) "pattern needs" else "patterns need"} your attention."
        reviewedFrames == 0 -> "There are no frames to review."
        else -> "No disconnect patterns found by the current checks."
    }
}

/** Baseline checks are explicit, not presented as a learned model or an exhaustive threat detector. */
object CaptureReviewer {
    fun review(evidence: CaptureEvidence): RecordingReview {
        val issues = evidence.episodes.mapIndexed { i, report ->
            val linked = report.followOnFrames.isNotEmpty()
            val repeated = report.disconnectFrames.size >= 3
            RecordingIssue("episode-${i + 1}", if (linked || repeated) ReviewLevel.SIGNIFICANT else ReviewLevel.REVIEW,
                when {
                    linked -> "Disconnect requests were followed by network access messages."
                    repeated -> "Repeated requests to disconnect a device were observed."
                    else -> "A request to disconnect was observed."
                }, report.explanation + " " + report.significance, report.evidence, report.target, report.network,
                report.disconnectFrames.size, report.followOnFrames.size, NetworkComparisons.read(report),report.networkNames)
        }.sortedWith(compareByDescending<RecordingIssue> { it.level }.thenBy { it.frameIds.firstOrNull() ?: 0 })
        return RecordingReview(evidence.captureId, evidence.frames.size, issues, evidence.coverage,
            evidence.followOnMs, evidence.episodeGapMs)
    }
}
