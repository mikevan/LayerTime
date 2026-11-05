package com.projectrevivesolutions.layertime.controller.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.LocalContext
import com.projectrevivesolutions.layertime.controller.link.EvidencePull
import com.projectrevivesolutions.layertime.controller.link.LinkEvidence
import com.projectrevivesolutions.layertime.controller.service.LinkForegroundService
import com.projectrevivesolutions.layertime.controller.state.ControllerRepository
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.projectrevivesolutions.layertime.controller.flow.Feature
import com.projectrevivesolutions.layertime.controller.flow.Finding
import com.projectrevivesolutions.layertime.controller.flow.Lean
import com.projectrevivesolutions.layertime.controller.flow.TriageSession
import com.projectrevivesolutions.layertime.controller.flow.Verdict
import com.projectrevivesolutions.layertime.controller.model.EventRow
import com.projectrevivesolutions.layertime.controller.ui.theme.LtColors
import com.projectrevivesolutions.layertime.controller.ui.theme.glowText
import com.projectrevivesolutions.layertime.controller.wireshark.PcapImport

/** The verdict's color: green, amber, red, red. */
fun verdictColor(v: Verdict): Color = when (v) {
    Verdict.NOT_WORRISOME -> LtColors.Green
    Verdict.WATCH -> LtColors.Amber
    Verdict.LIKELY_ATTACK, Verdict.ATTACK_WITH_FOLLOW_ON -> LtColors.Red
}

fun leanColor(l: Lean): Color = when (l) { Lean.BENIGN -> LtColors.Green; Lean.SUSPICIOUS -> LtColors.Amber; Lean.UNKNOWN -> LtColors.Grey }

/**
 * TRIAGE on the event detail (2026-11-04, slice 1): runs the shipped deauth triage
 * on the imported capture for this event and shows the Finding with its evidence
 * chain. Every sentence comes from the pipeline; this panel adds none.
 */
@Composable
fun TriageSection(e: EventRow, onOpenWireshark: () -> Unit) {
    val workflow by TriageSession.workflow.collectAsState()
    val problems by TriageSession.problems.collectAsState()
    val run by TriageSession.run.collectAsState()
    val runFor by TriageSession.runFor.collectAsState()
    val error by TriageSession.error.collectAsState()
    val capture by PcapImport.state.collectAsState()
    val server by ControllerRepository.server.collectAsState()
    val pull by EvidencePull.state.collectAsState()
    val context = LocalContext.current
    val haveCapture = capture is PcapImport.State.Done
    val mine = runFor == TriageSession.key(e)
    // Slice 2: the Hunter's own frames for this detection. A Wi-Fi detection on a
    // Hunter that keeps frames (capability bit 5) can be pulled, imported, and run.
    val keepsFrames = ((server?.capabilities ?: 0) and LinkEvidence.CAP_RAW_CAPTURE) != 0
    val wifi = e.sourceKind == 1
    val isDeauth = e.detectorName.equals("DEAUTH", ignoreCase = true)
    // null, "pulling", or "importing": this panel's own pull, so an older import is never mistaken for it.
    var stage by remember(e.eventId) { mutableStateOf<String?>(null) }
    val pcapName = "hunter_event_${e.eventId}.pcap"
    LaunchedEffect(pull, stage) {
        val p = pull
        if (stage == "pulling" && p is EvidencePull.State.Done && p.eventId == e.eventId) {
            stage = "importing"
            PcapImport.start(context, android.net.Uri.fromFile(p.file))
        }
        if (stage == "pulling" && p is EvidencePull.State.Failed && p.eventId == e.eventId) stage = null
    }
    LaunchedEffect(capture, stage) {
        val c = capture
        if (stage == "importing" && c is PcapImport.State.Done && c.fileName == pcapName) {
            stage = null
            if (isDeauth) TriageSession.runOnCapture(e)
        }
        if (stage == "importing" && c is PcapImport.State.Failed && c.fileName == pcapName) stage = null
    }
    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
        val w = workflow
        when {
            w == null -> Text("No workflow is loaded.", color = LtColors.Grey, fontSize = 12.sp)
            problems.isNotEmpty() -> Text("The workflow \"${w.name}\" does not validate: ${problems.first()}", color = LtColors.Red, fontSize = 12.sp)
            else -> {
                if (wifi && keepsFrames) {
                    Text(if (isDeauth) "Pull the frames the Hunter kept for this detection, then run ${w.name} on them."
                        else "Pull the frames the Hunter kept for this detection and open them in Wireshark. ${w.name} runs on deauth detections only.",
                        color = LtColors.Ivory, fontSize = 12.sp)
                    SciFiPrimary(if (isDeauth) "PULL THE HUNTER'S FRAMES AND RUN TRIAGE" else "PULL THE HUNTER'S FRAMES",
                        enabled = stage == null, large = true) {
                        EvidencePull.reset()
                        stage = "pulling"
                        LinkForegroundService.active?.pullEvidence(e.eventId)
                    }
                    when (val p = pull) {
                        is EvidencePull.State.Pulling -> if (p.eventId == e.eventId) Text(
                            if (p.expected == 0) "${p.phase} ${p.listed} listed so far."
                            else "${p.phase} ${p.bytes} of ${p.expected} bytes, ${p.listed} frames.", color = LtColors.Cyan, fontSize = 12.sp)
                        is EvidencePull.State.Done -> if (p.eventId == e.eventId) Text(
                            "Pulled ${p.frames} frames in ${p.elapsedMs} ms. The Hunter heard ${p.window.seen} matching frames and kept ${p.window.retained}" +
                                (if (p.window.dropped > 0) "; ${p.window.dropped} did not fit." else ".") +
                                (if (p.window.collecting) " The window was still collecting; pull again later for more." else ""),
                            color = LtColors.Green, fontSize = 12.sp)
                        is EvidencePull.State.Failed -> if (p.eventId == e.eventId) Text(p.message, color = LtColors.Red, fontSize = 12.sp)
                        EvidencePull.State.Idle -> {}
                    }
                    if (stage == "importing") Text("Decoding the frames with Wireshark.", color = LtColors.Cyan, fontSize = 12.sp)
                    if (!isDeauth && capture is PcapImport.State.Done && (capture as PcapImport.State.Done).fileName == pcapName)
                        SciFiOutline("OPEN THE FRAMES IN WIRESHARK", enabled = true, large = true) { TriageSession.focusFrames.value = null; onOpenWireshark() }
                } else {
                    Text(when {
                        !wifi -> "The Hunter keeps frames for Wi-Fi detections only. ${w.name} can still run on an imported capture."
                        server == null -> "Connect the Hunter to pull its frames, or run ${w.name} on an imported capture."
                        else -> "This Hunter's firmware does not keep frames. Flash the evidence firmware, or run ${w.name} on an imported capture."
                    }, color = LtColors.Amber, fontSize = 12.sp)
                    if (!haveCapture) Text("Import a capture on EVENTS first (IMPORT CAPTURE), then run the triage here.", color = LtColors.Amber, fontSize = 12.sp)
                    SciFiPrimary("RUN DEAUTH TRIAGE ON THE IMPORTED CAPTURE", enabled = haveCapture, large = true) { TriageSession.runOnCapture(e) }
                }
                if (mine) {
                    val f = run?.finding
                    if (f != null) FindingCard(f, onOpenWireshark)
                    else Text(error ?: "The run produced no finding.", color = LtColors.Red, fontSize = 12.sp)
                }
            }
        }
    }
}

@Composable
fun FindingCard(f: Finding, onOpenWireshark: () -> Unit) {
    val color = verdictColor(f.verdict)
    Column(Modifier.fillMaxWidth().background(color.copy(alpha = .08f)).padding(10.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        Text(f.verdict.label, color = color, fontWeight = FontWeight.Bold, fontSize = 20.sp, letterSpacing = 2.sp, style = glowText())
        Text(f.summary, color = LtColors.Ivory, fontSize = 13.sp)
        Text("EVIDENCE", color = LtColors.ConsoleEdge, fontWeight = FontWeight.Bold, fontSize = 12.sp, letterSpacing = 2.sp)
        for (x in f.features) FeatureRow(x)
        if (f.couldNotRun.isNotEmpty()) {
            Text("COULD NOT RUN", color = LtColors.Grey, fontWeight = FontWeight.Bold, fontSize = 12.sp, letterSpacing = 2.sp)
            for (x in f.couldNotRun) FeatureRow(x)
        }
        if (f.frames.isNotEmpty()) SciFiOutline("OPEN THE ${f.frames.size} FRAMES IN WIRESHARK", enabled = true, large = true) {
            TriageSession.focusFrames.value = f.frames.toSet(); onOpenWireshark()
        }
    }
}

@Composable
private fun FeatureRow(x: Feature) {
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.Top) {
        Box(Modifier.padding(top = 5.dp).size(8.dp).background(leanColor(x.lean)))
        Column(Modifier.weight(1f)) {
            Text("${x.title}: ${x.value}", color = leanColor(x.lean), fontWeight = FontWeight.Bold, fontSize = 12.sp)
            Text(x.reason, color = LtColors.Ivory, fontSize = 12.sp)
            if (x.frames.isNotEmpty()) Text("${x.frames.size} frame${if (x.frames.size == 1) "" else "s"}", color = LtColors.Grey, fontSize = 11.sp)
        }
    }
}

/**
 * TRIAGE on the capture page (2026-11-04): the same workflow and Finding as on an
 * event's detail, for a capture imported with no detection behind it.
 */
@Composable
fun CaptureTriageSection(onOpenWireshark: () -> Unit) {
    val workflow by TriageSession.workflow.collectAsState()
    val problems by TriageSession.problems.collectAsState()
    val run by TriageSession.run.collectAsState()
    val runFor by TriageSession.runFor.collectAsState()
    val error by TriageSession.error.collectAsState()
    val capture by PcapImport.state.collectAsState()
    val haveCapture = capture is PcapImport.State.Done
    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
        val w = workflow
        when {
            w == null -> Text("No workflow is loaded.", color = LtColors.Grey, fontSize = 12.sp)
            problems.isNotEmpty() -> Text("The workflow \"${w.name}\" does not validate: ${problems.first()}", color = LtColors.Red, fontSize = 12.sp)
            !haveCapture -> Text("Import a capture above, then run the triage here.", color = LtColors.Amber, fontSize = 12.sp)
            else -> {
                Text("Workflow: ${w.name} (version ${w.version}). Runs on this capture, looking at the transmitter that sent the most disconnect frames.",
                    color = LtColors.Ivory, fontSize = 12.sp)
                SciFiPrimary("RUN DEAUTH TRIAGE", enabled = true, large = true) { TriageSession.runOnCaptureAlone() }
                if (runFor == TriageSession.CAPTURE_KEY) {
                    TriageSession.captureSubject()?.let { Text("Transmitter: $it", color = LtColors.Cyan, fontSize = 12.sp) }
                    val f = run?.finding
                    if (f != null) FindingCard(f, onOpenWireshark)
                    else Text(error ?: "The run produced no finding.", color = LtColors.Red, fontSize = 12.sp)
                }
            }
        }
    }
}
