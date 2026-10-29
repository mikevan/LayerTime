package com.projectrevivesolutions.layertime.controller.ui

import android.net.Uri
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyListScope
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CutCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.snapshots.SnapshotStateMap
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.projectrevivesolutions.layertime.controller.link.LinkConstants
import com.projectrevivesolutions.layertime.controller.model.ConnState
import com.projectrevivesolutions.layertime.controller.model.EventRow
import com.projectrevivesolutions.layertime.controller.ui.theme.LtColors
import com.projectrevivesolutions.layertime.controller.ui.theme.ConsoleShape
import com.projectrevivesolutions.layertime.controller.ui.theme.consoleFrame
import com.projectrevivesolutions.layertime.controller.ui.theme.hudPanel
import com.projectrevivesolutions.layertime.controller.ui.theme.glowText
import com.projectrevivesolutions.layertime.controller.ui.theme.ConsoleFonts
import com.projectrevivesolutions.layertime.controller.ui.theme.railBrush
import com.projectrevivesolutions.layertime.controller.service.PcapImportService
import com.projectrevivesolutions.layertime.controller.wireshark.PcapImport
import com.projectrevivesolutions.layertime.controller.wireshark.WiresharkDecoder
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Provenance of a field, so decoded facts, receiver metadata, calculated
 * statistics, and inferred conclusions are never confused with one another.
 */
private enum class Prov(val label: String, val color: Color) {
    DECODED("DECODED", LtColors.Green),       // a value read out of a frame or its text
    METADATA("METADATA", LtColors.Cyan),      // supplied by the wand's receiver (rx_ctrl)
    CALC("CALCULATED", LtColors.Amber),       // a statistic computed here
    INFERRED("INFERRED", Color(0xFFF5B83D)),  // a conclusion the wand's classifier drew
    UNAVAIL("UNAVAILABLE", LtColors.Grey),    // not present, with a reason
}

private val tsFmt = SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS", Locale.US)
private val sectionShape = ConsoleShape

/**
 * Event-details screen for one detection. Shows every section the spec calls
 * for. Because Link 0.1 transports only the normalized EVENT_SUMMARY and not
 * the captured 802.11 frames, address, frame-header, radiotap, and raw-hex
 * fields are marked UNAVAILABLE with the reason. Nothing here is fabricated.
 *
 * Section 6 can import a capture file the user selects and decode every frame
 * with Wireshark on the phone. Those decoded fields come from the imported
 * file, not from this event, and the screen says so.
 */
@Composable
fun EventDetailScreen(
    e: EventRow,
    wandLabel: String,
    conn: ConnState,
    onBack: () -> Unit,
) {
    val noFrames = "Raw 802.11 frames are not transported by LayerTime Link 0.1 (EVENT_SUMMARY is normalized). See the Link 0.2 evidence-transfer proposal."
    val capture = rememberCaptureImportUi()
    LazyColumn(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        item {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                SciFiOutline("‹ BACK", enabled = true) { onBack() }
                Text("ATTACK DETAIL  //  #${e.eventId}  ${e.detectorName}", color = LtColors.ConsoleEdge,
                    fontWeight = FontWeight.Bold, fontSize = 14.sp, letterSpacing = 1.sp, style = glowText())
            }
        }

        item {
            Section("1 - DETECTION SUMMARY") {
                Field("Classification", e.detectorName, Prov.INFERRED, "The wand's on-board classifier concluded this; it is not a decoded frame-type field.")
                Field("Observed behavior", e.detail, Prov.DECODED, "Short text the wand observed over the air; untrusted.")
                Field("Confidence", LinkConstants.Confidence.name(e.confidence), Prov.INFERRED)
                Field("Confidence basis", null, Prov.UNAVAIL, "Not transported by Link 0.1.")
                Field("First observed (phone)", tsFmt.format(Date(e.firstSeenMillis)), Prov.CALC, "Phone receive time, not the wand's capture time.")
                Field("Last observed (phone)", tsFmt.format(Date(e.lastUpdatedMillis)), Prov.CALC, "Phone receive time.")
                Field("Observation window", "${(e.lastUpdatedMillis - e.firstSeenMillis) / 1000}s", Prov.CALC, "Across phone receive times.")
                Field("Repeated sightings", "${e.count}", Prov.CALC, "EVENT_SUMMARY count, saturating at 65535.")
                Field("Age at last report", "${e.ageSeconds}s", Prov.CALC)
                Field("Captured frame count", null, Prov.UNAVAIL, "Link 0.1 reports a sighting count, not a captured-frame count.")
                Field("Measured frame rate", null, Prov.UNAVAIL, "No per-frame timing crosses the link.")
                Field("Originating wand", wandLabel.ifBlank { "unknown" }, Prov.METADATA)
                Field("Wand connection", conn.name, Prov.METADATA)
                Note("Repeated sightings and aggregated events are the same EVENT_SUMMARY count here. They are not captured frames, which Link 0.1 does not carry.")
            }
        }

        item {
            Section("2 - ADDRESSES AND TARGETS") {
                Field("Transmitter (TA)", null, Prov.UNAVAIL, noFrames)
                Field("Receiver (RA)", null, Prov.UNAVAIL, noFrames)
                Field("Source (SA)", null, Prov.UNAVAIL, noFrames)
                Field("Destination (DA)", null, Prov.UNAVAIL, noFrames)
                Field("BSSID", null, Prov.UNAVAIL, noFrames)
                Field("Broadcast / multicast", null, Prov.UNAVAIL, noFrames)
                Field("SSID", if (e.sourceId.isNullOrBlank()) null else e.sourceId, Prov.DECODED,
                    "From the wand's source-id text, observed over the air and untrusted. Not correlated to a specific management frame here.")
                Field("Vendor (OUI) lookup", null, Prov.UNAVAIL, "No MAC address is available. A match would be an address-prefix lookup, not verified device identity.")
            }
        }

        item {
            Section("3 - FRAME DETAILS") {
                Field("Type / subtype", null, Prov.UNAVAIL, noFrames)
                Field("Reason code", null, Prov.UNAVAIL, noFrames)
                Field("Sequence / fragment", null, Prov.UNAVAIL, noFrames)
                Field("Retry flag", null, Prov.UNAVAIL, noFrames)
                Field("Protected-frame flag", null, Prov.UNAVAIL, noFrames)
                Field("Frame length", null, Prov.UNAVAIL, noFrames)
                Field("FCS present / valid", null, Prov.UNAVAIL, noFrames)
                Field("Information elements", null, Prov.UNAVAIL, noFrames)
                Note("A reason code states the sender's claimed reason. It is not proof of an attack.")
            }
        }

        item {
            Section("4 - RADIO AND CAPTURE METADATA") {
                Field("Channel", "${e.channel}", Prov.METADATA, "From the wand receiver (rx_ctrl.channel), 0 when unknown or BLE.")
                Field("Band", LinkConstants.Band.name(e.band), Prov.METADATA)
                Field("RSSI (last seen)", "${e.rssi} dBm", Prov.METADATA, "Single last-seen value, not a series.")
                Field("Noise / rate / PHY / antenna", null, Prov.UNAVAIL, "The ESP32 receiver does not supply these over Link 0.1; no Radiotap is fabricated.")
                Field("Per-frame timestamp", null, Prov.UNAVAIL, "Only ages in seconds cross the link; timestamps do not.")
                Field("Capture source", "Wand promiscuous Wi-Fi monitor", Prov.METADATA)
                Field("Captured vs original length", null, Prov.UNAVAIL, noFrames)
                Field("Dropped-frame count", null, Prov.UNAVAIL, "Not transported by Link 0.1.")
            }
        }

        item {
            Section("5 - TIMELINE AND MULTI-WAND") {
                Field("Frame-rate plot", null, Prov.UNAVAIL, "No per-frame series is available; only a sighting count and a last-seen RSSI.")
                Field("RSSI plot", null, Prov.UNAVAIL, "Only one last-seen RSSI value is available.")
                Field("Burst timing / retries", null, Prov.UNAVAIL, noFrames)
                Field("Wands observing", "1", Prov.METADATA, "Single wand this build; multi-wand correlation lands with the multi-wand work.")
                Field("Clock sync quality", null, Prov.UNAVAIL, "No cross-wand clock synchronization yet; timestamps are not compared.")
                Note("Distance, bearing, location, and verified attacker identity are not derived from MAC or RSSI alone.")
            }
        }

        item {
            Section("6 - DECODED EVIDENCE") {
                Field("Protocol tree for this event", null, Prov.UNAVAIL, noFrames)
                Field("Raw frame hex for this event", null, Prov.UNAVAIL, noFrames)
                Field("Export PCAP / PCAPNG", null, Prov.UNAVAIL, "No raw frames from the link to write.")
                Note("A structured JSON report of all detections is available from EXPORT DIAGNOSTICS on the main screen.")
            }
        }

        captureImportItems(capture, "7 - IMPORTED CAPTURE (WIRESHARK)", e.eventId)
    }
}

/** Screen state for capture import, shared by the event-detail screen and the capture-only screen. */
class CaptureImportUi(
    val state: PcapImport.State,
    val expanded: SnapshotStateMap<Int, Boolean>,
    val pick: () -> Unit,
)

@Composable
fun rememberCaptureImportUi(): CaptureImportUi {
    val context = LocalContext.current
    val state by PcapImport.state.collectAsState()
    val expanded = remember { mutableStateMapOf<Int, Boolean>() }
    val picker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri: Uri? ->
        android.util.Log.i(PcapImport.TAG, "PICKER RESULT uri=${uri ?: "none (cancelled or no file)"}")
        if (uri != null) { expanded.clear(); PcapImportService.importFile(context, uri) }
    }
    return CaptureImportUi(state, expanded) {
        android.util.Log.i(PcapImport.TAG, "IMPORT tapped; launching the system file picker")
        runCatching { picker.launch(arrayOf("*/*")) }
            .onFailure { android.util.Log.e(PcapImport.TAG, "PICKER LAUNCH FAILED: ${it.javaClass.simpleName}: ${it.message}") }
    }
}

/** The capture-import section and, after a decode, one card per kept frame. [eventId] is null when no detection is open. */
fun LazyListScope.captureImportItems(ui: CaptureImportUi, title: String, eventId: Long?) {
    item {
        Section(title) {
            Text("Decode a .pcap or .pcapng file with Wireshark on this device. " +
                (if (eventId != null) "It is not linked to event #$eventId." else "It is not linked to any detection."),
                color = LtColors.Grey, fontSize = 11.sp)
            when (val st = ui.state) {
                is PcapImport.State.Idle -> {
                    SciFiOutline("IMPORT CAPTURE FILE", enabled = true, large = true) { ui.pick() }
                }
                is PcapImport.State.Running -> {
                    val what = when (st.phase) {
                        "copy" -> "Copying ${st.fileName} into app storage."
                        "init" -> "Loading Wireshark. The first load takes a few seconds."
                        else -> "Decoding " + st.fileName + ": " +
                            "%,d frames%s, %.0f frames per second, %.0f s elapsed."
                                .format(st.frames, st.percent?.let { " ($it% of the file)" } ?: "", st.ratePerSecond, st.elapsedSeconds)
                    }
                    Text(what, color = LtColors.Green, fontSize = 11.sp, fontFamily = ConsoleFonts.mono)
                    if (st.slowing) Text("Decoding has slowed to less than half its starting rate. It is still running.",
                        color = LtColors.Amber, fontSize = 11.sp)
                    SciFiOutline("CANCEL IMPORT", enabled = true, large = true) { PcapImport.cancel() }
                }
                is PcapImport.State.Failed -> {
                    Text("${st.fileName}: ${st.message}", color = LtColors.Red, fontSize = 11.sp)
                    SciFiOutline("IMPORT CAPTURE FILE", enabled = true, large = true) { ui.pick() }
                }
                is PcapImport.State.Done -> {
                    val r = st.result
                    Field("Imported file", st.fileName, Prov.METADATA, "%,d bytes, selected by you on this device.".format(st.fileBytes))
                    Field("File format", st.fileType, Prov.METADATA, "As identified by Wireshark.")
                    Field("Decoder", "Wireshark ${st.wiresharkVersion}", Prov.METADATA, "libwireshark/epan running on this device.")
                    Field("Frames decoded", "%,d".format(r.totalFrames), Prov.CALC,
                        if (r.cancelled) "Import was cancelled. Only frames read before cancelling are counted." else null)
                    Field("Frames shown below", "%,d".format(r.kept.size), Prov.CALC,
                        if (r.totalFrames > r.kept.size) "Only the first ${r.keepLimit} frames keep their full protocol tree." else null)
                    Field("Malformed frames", "%,d".format(r.malformedFrames), Prov.DECODED, "Frames Wireshark marked as malformed or truncated.")
                    Field("Decode time", "%.1f s".format(r.elapsedSeconds), Prov.CALC)
                    r.readError?.let { Text("Wireshark stopped reading the file: $it", color = LtColors.Red, fontSize = 11.sp) }
                    Note("Each frame is decoded on its own; no reassembly across frames.")
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        SciFiOutline("IMPORT ANOTHER FILE", enabled = true, large = true) { ui.pick() }
                        SciFiOutline("CLEAR", enabled = true, large = true) { ui.expanded.clear(); PcapImport.clear() }
                    }
                }
            }
        }
    }

    val done = ui.state as? PcapImport.State.Done
    if (done != null) {
        items(done.result.kept, key = { it.number }) { f ->
            FrameCard(f, ui.expanded[f.number] == true) { ui.expanded[f.number] = !(ui.expanded[f.number] ?: false) }
        }
    }
}

/**
 * Decode a capture file with no detection open, reached from DECODE A CAPTURE FILE
 * on the detected-attacks page. Same import and display as section 7 of the
 * event-detail screen.
 */
@Composable
fun CaptureOnlyScreen(onBack: () -> Unit) {
    val capture = rememberCaptureImportUi()
    LazyColumn(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        item {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                SciFiOutline("‹ BACK", enabled = true) { onBack() }
                Text("CAPTURE DECODE  //  NO DETECTION SELECTED", color = LtColors.ConsoleEdge,
                    fontWeight = FontWeight.Bold, fontSize = 14.sp, letterSpacing = 1.sp, style = glowText())
            }
        }
        captureImportItems(capture, "IMPORTED CAPTURE (WIRESHARK)", null)
    }
}

/** One imported frame. Collapsed: a one-line summary. Expanded: every Wireshark field with its byte offset, then the bytes. */
@Composable
private fun FrameCard(f: WiresharkDecoder.PcapFrame, open: Boolean, onToggle: () -> Unit) {
    Box(
        Modifier.fillMaxWidth().padding(horizontal = 2.dp, vertical = 3.dp)
            .hudPanel(edge = if (f.malformed) LtColors.Red else LtColors.Green, radius = 16.dp, elevation = 4.dp)
            .clip(ConsoleShape).clickable { onToggle() }.padding(19.dp)
    ) {
        Column(verticalArrangement = Arrangement.spacedBy(3.dp)) {
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                Text((if (open) "▾ " else "▸ ") + "#${f.number}", color = LtColors.Cyan, fontSize = 11.sp,
                    fontFamily = ConsoleFonts.mono, fontWeight = FontWeight.Bold)
                Text(f.title, color = LtColors.Ivory, fontSize = 11.sp, fontWeight = FontWeight.Bold)
                ProvTag(Prov.DECODED)
                if (f.malformed) Text("MALFORMED", color = LtColors.Red, fontSize = 9.sp, letterSpacing = 1.sp)
            }
            val sub = listOf(f.addresses, "${f.capturedLength} bytes",
                if (f.hasTimestamp) tsFmt.format(Date(f.epochSeconds * 1000 + f.nanos / 1_000_000)) + " (capture time)" else "")
                .filter { it.isNotBlank() }.joinToString("   ")
            Text(sub, color = LtColors.Grey, fontSize = 10.sp, fontFamily = ConsoleFonts.mono)
            if (open) {
                Text("Wireshark fields (name, value, byte offset, length):", color = LtColors.Cyan, fontSize = 10.sp)
                f.fields.forEach { fd ->
                    Row(Modifier.fillMaxWidth().padding(start = (fd.depth * 8).dp)) {
                        Text(
                            "${fd.name.ifBlank { "(text)" }}  ${fd.shown}" +
                                (if (fd.start >= 0 && fd.length > 0) "  @${fd.start}+${fd.length}" else ""),
                            color = if (fd.name.startsWith("_ws.malformed")) LtColors.Red else LtColors.Ivory,
                            fontSize = 10.sp, fontFamily = ConsoleFonts.mono)
                    }
                }
                if (f.hex.isNotEmpty()) {
                    Text("Frame bytes (offset, hex):", color = LtColors.Cyan, fontSize = 10.sp)
                    WiresharkDecoder.hexDump(f.hex).forEach {
                        Text(it, color = LtColors.Ivory, fontSize = 10.sp, fontFamily = ConsoleFonts.mono)
                    }
                }
            }
        }
    }
}

@Composable
private fun ProvTag(prov: Prov) {
    Box(Modifier.background(prov.color.copy(alpha = 0.18f), sectionShape)
        .border(1.dp, prov.color.copy(alpha = 0.6f), sectionShape)
        .padding(horizontal = 5.dp)) {
        Text(prov.label, color = prov.color, fontSize = 8.sp, letterSpacing = 1.sp)
    }
}

@Composable
private fun Section(title: String, content: @Composable () -> Unit) {
    Box(
        Modifier.fillMaxWidth().padding(horizontal = 2.dp, vertical = 3.dp).hudPanel().padding(21.dp)
    ) {
        Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Text(title, color = LtColors.ConsoleEdge, fontWeight = FontWeight.Bold, fontSize = 12.sp, letterSpacing = 2.sp, style = glowText())
            Box(Modifier.fillMaxWidth().height(3.dp).background(railBrush(LtColors.ConsoleEdge)))
            content()
        }
    }
}

@Composable
private fun Field(label: String, value: String?, prov: Prov, reason: String? = null) {
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Text(label, color = LtColors.Grey, fontSize = 11.sp, modifier = Modifier.width(150.dp),
            fontFamily = ConsoleFonts.mono)
        Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(1.dp)) {
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                Text(value ?: "UNAVAILABLE",
                    color = if (value != null) LtColors.Ivory else LtColors.Grey,
                    fontSize = 12.sp, fontWeight = FontWeight.Bold, fontFamily = ConsoleFonts.mono)
                Box(Modifier.background(prov.color.copy(alpha = 0.18f), sectionShape)
                    .border(1.dp, prov.color.copy(alpha = 0.6f), sectionShape)
                    .padding(horizontal = 5.dp)) {
                    Text(prov.label, color = prov.color, fontSize = 8.sp, letterSpacing = 1.sp)
                }
            }
            if (reason != null) Text(reason, color = LtColors.Grey, fontSize = 10.sp)
        }
    }
}

@Composable
private fun Note(text: String) {
    Text(text, color = LtColors.Amber, fontSize = 10.sp, fontWeight = FontWeight.Medium)
}
