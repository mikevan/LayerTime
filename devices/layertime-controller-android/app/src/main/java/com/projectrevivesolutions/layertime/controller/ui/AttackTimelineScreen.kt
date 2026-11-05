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
fun AttackTimelineScreen(onBack: () -> Unit) {
    val capture = rememberCaptureImportUi()
    val st = capture.state
    val done = st as? PcapImport.State.Done
    val stats: List<TransmitterStats> = remember(done) {
        val frames = done?.result?.kept.orEmpty().mapNotNull { toTimelineFrame(it) }
        AttackTimeline.summarize(frames)
    }
    LazyColumn(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        item {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                SciFiOutline("‹ BACK", enabled = true) { onBack() }
                Text("ATTACK TIMELINE  //  PER TRANSMITTER", color = LtColors.ConsoleEdge,
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
                    "A sequence break that is not a retransmission is the tell of an injected frame.",
                    color = LtColors.Grey, fontSize = 12.sp)
            }
            items(stats, key = { it.transmitter }) { s -> TransmitterCard(s) }
        }
    }
}

@Composable
private fun TransmitterCard(s: TransmitterStats) {
    val suspicious = s.seqAnomalies > 0
    val edge = if (suspicious) LtColors.Red else LtColors.Green
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
        if (suspicious) Text("Sequence continuity broken. Frames claiming this address came from more than one radio.",
            color = LtColors.Red, fontSize = 12.sp)
    }
}
