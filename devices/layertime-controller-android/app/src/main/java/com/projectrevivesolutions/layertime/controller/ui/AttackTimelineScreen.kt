package com.projectrevivesolutions.layertime.controller.ui

import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.projectrevivesolutions.layertime.controller.ui.theme.ConsoleFonts
import com.projectrevivesolutions.layertime.controller.ui.theme.LtColors
import com.projectrevivesolutions.layertime.controller.ui.theme.glowText
import com.projectrevivesolutions.layertime.controller.flow.AnalyticFrame
import com.projectrevivesolutions.layertime.controller.flow.TargetBehavior
import com.projectrevivesolutions.layertime.controller.wireshark.AttackTimeline
import com.projectrevivesolutions.layertime.controller.wireshark.PcapImport
import com.projectrevivesolutions.layertime.controller.wireshark.TimelineFrame
import com.projectrevivesolutions.layertime.controller.wireshark.TransmitterStats
import com.projectrevivesolutions.layertime.controller.wireshark.WiresharkDecoder

private fun leadingInt(s: String?): Int? = s?.trim()?.takeWhile { it.isDigit() }?.toIntOrNull()

/** Pull the fields the timeline needs from one decoded frame; null if it has no transmitter. */
private fun toTimelineFrame(f: WiresharkDecoder.PcapFrame): TimelineFrame? {
    val tx = f.first("wlan.sa")?.display ?: f.first("wlan.ta")?.display ?: return null
    val sub = f.first("wlan.fc.type_subtype")?.shown ?: f.title
    val seq = leadingInt(f.first("wlan.seq")?.display)
    val retry = f.first("wlan.fc.retry")?.display?.let { it.trim() == "1" } ?: false
    val reason = leadingInt(f.first("wlan.fixed.reason_code")?.display)
    return TimelineFrame(tx, f.epochSeconds * 1_000_000_000L + f.nanos, sub, seq, retry, reason)
}

/**
 * The attack-timeline window (claude/layerscope_analytics_design.md, analytics window 2). It reads
 * the capture already decoded in TOOLS (shared PcapImport state) and shows, per transmitter, the
 * signals that separate housekeeping from an attack: rate, reason codes, retransmissions, and the
 * sequence-number breaks that mark forged frames. Passive: it describes, it never claims a response.
 */
@Composable
fun AttackTimelineScreen(onBack: () -> Unit, onOpenWireshark: () -> Unit = {}) {
    val capture = rememberCaptureImportUi()
    val st = capture.state
    val done = st as? PcapImport.State.Done
    val stats: List<TransmitterStats> = remember(done) {
        val frames = done?.result?.kept.orEmpty().mapNotNull { toTimelineFrame(it) }
        AttackTimeline.summarize(frames)
    }
    val reports = remember(done) {
        TargetBehavior.analyze(done?.result?.observations.orEmpty())
    }
    LazyColumn(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        item {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                if(com.projectrevivesolutions.layertime.controller.DeviceKind.isLokmatWatch) SciFiOutline("‹ BACK", enabled = true) { onBack() }
                Text("BEHAVIOR TIMELINE", color = LtColors.ConsoleEdge,
                    fontWeight = FontWeight.Bold, fontSize = 16.sp, letterSpacing = 1.sp, style = glowText())
            }
        }
        if (done == null) {
            item {
                Text("Decode a capture in the Wireshark window first; the timeline reads the same frames.",
                    color = LtColors.Grey, fontSize = 12.sp)
                SciFiPrimary("IMPORT CAPTURE FILE", enabled = true, large = true) { capture.pick() }
            }
        } else {
            item {
                Text("${stats.size} transmitters across ${done.result.kept.size} decoded frames. " +
                    "Sequence gaps alone do not establish injection.",
                    color = LtColors.Grey, fontSize = 12.sp)
            }
            item {
                if(done.training) Text("TRAINING DATA - generated example, not live evidence.",color=LtColors.Amber,fontSize=13.sp)
                Text("BY TARGET AND NETWORK", color = LtColors.Cyan, fontSize = 14.sp)
                Text("Network comparison: 30 seconds before and after a disconnect. Follow-on links require later traffic for the same device and access point. Reports cover ${done.result.observations.size} of ${done.result.totalFrames} decoded frames. Addresses are observed identifiers, not verified identities.", color = LtColors.Grey, fontSize = 12.sp)
                if (reports.isEmpty()) Text("No disconnect frames were observed. This is not an assessment of safety.", color = LtColors.Grey, fontSize = 12.sp)
            }
            items(reports) { report ->
                Column(Modifier.fillMaxWidth().border(1.dp, LtColors.Cyan, RoundedCornerShape(14.dp)).padding(16.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    Text("${report.target ?: "Broadcast / unresolved client"} · ${report.network ?: "Unknown network"}", color = LtColors.Amber, fontSize = 13.sp)
                    val comparison=com.projectrevivesolutions.layertime.controller.flow.NetworkComparisons.read(report)
                    Text(comparison.headline,color=LtColors.Cyan,fontSize=16.sp)
                    Text(comparison.order,color=LtColors.Ivory,fontSize=15.sp)
                    Text("Network name: ${report.networkNames.joinToString().ifEmpty {"not captured"}}",color=LtColors.Ivory,fontSize=14.sp)
                    Text(comparison.explanation,color=LtColors.Ivory,fontSize=13.sp)
                    if(report.eapolNetworkCandidates.isNotEmpty())SciFiOutline("COMPARE NETWORK EVIDENCE",true) {
                        com.projectrevivesolutions.layertime.controller.flow.TriageSession.focusFrames.value=comparison.evidence.toSet()
                        onOpenWireshark()
                    }
                    Text(report.explanation, color = LtColors.Ivory, fontSize = 12.sp)
                    SciFiOutline("INSPECT THIS EPISODE",enabled=report.evidence.isNotEmpty()) {
                        com.projectrevivesolutions.layertime.controller.flow.TriageSession.focusFrames.value=report.evidence.toSet()
                        onOpenWireshark()
                    }
                    Text(report.significance, color = LtColors.Grey, fontSize = 12.sp)
                    Text("Capture positions (1-based): disconnect ${report.disconnectFrames.joinToString { (it + 1).toString() }}; follow-on ${report.followOnFrames.joinToString { (it + 1).toString() }.ifEmpty { "none observed" }}", color = LtColors.Cyan, fontSize = 11.sp)
                }
            }
            item { Text("TRANSMITTER STATISTICS", color = LtColors.Cyan, fontSize = 14.sp) }
            items(stats, key = { it.transmitter }) { s -> TransmitterCard(s) }
        }
    }
}

@Composable
private fun TransmitterCard(s: TransmitterStats) {
    val suspicious = s.seqAnomalies > 0
    val edge = if (suspicious) LtColors.Amber else LtColors.Cyan
    Column(
        Modifier.fillMaxWidth().padding(horizontal = 2.dp, vertical = 3.dp)
            .border(1.5.dp, edge, RoundedCornerShape(14.dp)).padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(4.dp),
    ) {
        Text(s.transmitter, color = LtColors.Amber, fontWeight = FontWeight.Bold, fontSize = 14.sp,
            fontFamily = ConsoleFonts.mono)
        Text("%,d frames  ·  %.1f frames/sec  ·  %.2f s".format(s.frames, s.framesPerSecond, s.spanSeconds),
            color = LtColors.Ivory, fontSize = 12.sp, fontFamily = ConsoleFonts.mono)
        Text("SEQ BREAKS ${s.seqAnomalies}   RETRIES ${s.retries}", color = if (suspicious) LtColors.Red else LtColors.Cyan,
            fontWeight = FontWeight.Bold, fontSize = 12.sp, fontFamily = ConsoleFonts.mono)
        if (s.minGapMs != null) {
            Text("GAP min %.1f ms  ·  median %.1f ms".format(s.minGapMs, s.medianGapMs ?: s.minGapMs),
                color = LtColors.Grey, fontSize = 12.sp, fontFamily = ConsoleFonts.mono)
        }
        if (s.reasonCodes.isNotEmpty()) {
            Text("REASON CODES  " + s.reasonCodes.entries.sortedByDescending { it.value }
                .joinToString("  ") { "${it.key}×${it.value}" },
                color = LtColors.Ivory, fontSize = 12.sp, fontFamily = ConsoleFonts.mono)
        }
        Text(s.subtypes.entries.sortedByDescending { it.value }.take(4).joinToString("  ") { "${it.key} ×${it.value}" },
            color = LtColors.Grey, fontSize = 12.sp)
        if (suspicious) Text("Sequence gaps observed. Capture loss and separate sequence streams can also explain gaps; compare other evidence.",
            color = LtColors.Red, fontSize = 12.sp)
    }
}
