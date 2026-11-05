package com.projectrevivesolutions.layertime.controller.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.projectrevivesolutions.layertime.controller.ui.theme.ConsoleFonts
import com.projectrevivesolutions.layertime.controller.ui.theme.LtColors
import com.projectrevivesolutions.layertime.controller.ui.theme.glowText
import com.projectrevivesolutions.layertime.controller.wireshark.FrameFacts
import com.projectrevivesolutions.layertime.controller.wireshark.FrameGlossary
import com.projectrevivesolutions.layertime.controller.wireshark.PcapImport
import com.projectrevivesolutions.layertime.controller.wireshark.WiresharkFilter

/**
 * The on-device Wireshark window (claude/layerscope_analytics_design.md, analytics window 1).
 * It reuses the existing capture-import pipeline (rememberCaptureImportUi / PcapImport) and the
 * per-frame decode cards (FrameCard: protocol tree with byte offsets, then the hex), and adds
 * what the window was missing: a display filter typed on the spot, over the packet list.
 *
 * Capture files first; the live tap feeds this same view once Link 0.2 throughput is measured.
 */
@Composable
fun WiresharkScreen(onBack: () -> Unit) {
    val capture = rememberCaptureImportUi()
    var filter by remember { mutableStateOf("") }
    val st = capture.state
    val done = st as? PcapImport.State.Done
    val all = done?.result?.kept.orEmpty()
    // A finding can ask for just the frames behind it (TriageSession.focusFrames); CLEAR shows everything again.
    val focus by com.projectrevivesolutions.layertime.controller.flow.TriageSession.focusFrames.collectAsState()
    val frames = remember(all, focus) { focus?.let { set -> all.filterIndexed { i, _ -> i in set } } ?: all }
    val shown = remember(frames, filter) {
        frames.filter { f ->
            WiresharkFilter.matches(f.title, f.addresses, f.fields.map { "${it.name} ${it.shown}" }, filter)
        }
    }
    LazyColumn(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        item {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                SciFiOutline("‹ BACK", enabled = true) { onBack() }
                Text("WIRESHARK  //  CAPTURE DECODE", color = LtColors.ConsoleEdge,
                    fontWeight = FontWeight.Bold, fontSize = 16.sp, letterSpacing = 1.sp, style = glowText())
            }
        }
        item {
            when (st) {
                is PcapImport.State.Idle ->
                    SciFiPrimary("IMPORT CAPTURE FILE", enabled = true, large = true) { capture.pick() }
                is PcapImport.State.Running -> Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    val what = when (st.phase) {
                        "copy" -> "Copying ${st.fileName} into app storage."
                        "init" -> "Loading Wireshark. The first load takes a few seconds."
                        else -> "Decoding ${st.fileName}: %,d frames, %.0f frames per second, %.0f s elapsed."
                            .format(st.frames, st.ratePerSecond, st.elapsedSeconds)
                    }
                    Text(what, color = LtColors.Green, fontSize = 12.sp, fontFamily = ConsoleFonts.mono)
                    SciFiOutline("CANCEL IMPORT", enabled = true, large = true) { PcapImport.cancel() }
                }
                is PcapImport.State.Failed -> Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    Text("${st.fileName}: ${st.message}", color = LtColors.Red, fontSize = 12.sp)
                    SciFiPrimary("IMPORT CAPTURE FILE", enabled = true, large = true) { capture.pick() }
                }
                is PcapImport.State.Done -> Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    Text("${st.fileName}  ·  Wireshark ${st.wiresharkVersion}  ·  " +
                        "%,d frames (%,d decoded with full tree)".format(st.result.totalFrames, st.result.kept.size),
                        color = LtColors.Ivory, fontSize = 12.sp, fontFamily = ConsoleFonts.mono)
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        SciFiOutline("IMPORT ANOTHER FILE", enabled = true, large = true) { capture.pick() }
                        SciFiOutline("CLEAR", enabled = true, large = true) { capture.expanded.clear(); PcapImport.clear() }
                    }
                }
            }
        }
        if (done != null) {
            item {
                Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    OutlinedTextField(
                        value = filter, onValueChange = { filter = it }, singleLine = true,
                        modifier = Modifier.fillMaxWidth(),
                        label = { Text("DISPLAY FILTER", color = LtColors.Cyan, fontSize = 12.sp) },
                        textStyle = TextStyle(color = LtColors.Ivory, fontFamily = ConsoleFonts.mono),
                    )
                    Text("Showing ${shown.size} of ${frames.size} frames.  Text filter over the title, " +
                        "addresses, and every decoded field; space-separated terms are ANDed.",
                        color = LtColors.Grey, fontSize = 12.sp)
                    focus?.let { set ->
                        Text("Only the ${set.size} frames behind the finding are listed, out of ${all.size} in the capture.", color = LtColors.Amber, fontSize = 12.sp)
                        SciFiOutline("SHOW THE WHOLE CAPTURE", enabled = true) { com.projectrevivesolutions.layertime.controller.flow.TriageSession.focusFrames.value = null }
                    }
                }
            }
            items(shown, key = { it.number }) { f ->
                // What this frame type means, above every card (Michael, 2026-11-04).
                val meaning = remember(f) { FrameGlossary.explain(FrameFacts.of(f)) }
                Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
                    Text(meaning, color = LtColors.Cyan, fontSize = 12.sp, modifier = Modifier.padding(horizontal = 4.dp))
                    FrameCard(f, capture.expanded[f.number] == true) {
                        capture.expanded[f.number] = !(capture.expanded[f.number] ?: false)
                    }
                }
            }
        }
    }
}
