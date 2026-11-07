package com.projectrevivesolutions.layertime.controller.ui

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.provider.DocumentsContract
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.projectrevivesolutions.layertime.controller.card.HunterCardLog
import com.projectrevivesolutions.layertime.controller.link.CardPull
import com.projectrevivesolutions.layertime.controller.service.LinkForegroundService
import androidx.compose.runtime.collectAsState
import com.projectrevivesolutions.layertime.controller.service.PcapImportService
import com.projectrevivesolutions.layertime.controller.ui.theme.ConsoleFonts
import com.projectrevivesolutions.layertime.controller.ui.theme.LtColors
import com.projectrevivesolutions.layertime.controller.ui.theme.glowText
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

/**
 * IMPORT HUNTER CARD (Michael, 2026-11-04). Two sources, read only on the Hunter:
 *  - PULL FROM THE HUNTER: the connected Hunter sends its card over the Link (CARD_LIST,
 *    CARD_READ), into this app's own storage. The field path: no computer, no cable.
 *  - A picked folder: the card in a USB card reader, or a folder it was copied to. The
 *    grant is kept, so it is picked once.
 * Either way LayerScope lists the hunter_NNNN.log files, reads one (HunterCardLog), and
 * opens a detection's saved frames (ev_NNNN/event_EEEEE.pcap) in the Wireshark window.
 */
@Composable
fun HunterCardScreen(onBack: () -> Unit, onOpenWireshark: () -> Unit) {
    val context = LocalContext.current
    var tree by remember { mutableStateOf(CardPrefs.readTree(context)) }
    // Files pulled over the Link live here; this source is used when tree is null.
    val pulledDir = remember { java.io.File(context.filesDir, "hunter_card") }
    var usePulled by remember { mutableStateOf(tree == null && pulledDir.exists()) }
    var reloads by remember { mutableStateOf(0) }
    val pull by CardPull.state.collectAsState()
    // The Link client is not observable, so whether it serves the card is checked each second.
    var servesCard by remember { mutableStateOf(false) }
    var noSdLog by remember { mutableStateOf<Boolean?>(null) }
    LaunchedEffect(Unit) {
        while (true) {
            val client = LinkForegroundService.active
            servesCard = client?.servesCard == true
            noSdLog = client?.lastNoSdLog
            kotlinx.coroutines.delay(1000)
        }
    }
    val offer = CardPull.offer(servesCard, noSdLog)
    var logs by remember { mutableStateOf<List<CardFile>>(emptyList()) }
    var chosen by remember { mutableStateOf<CardFile?>(null) }
    var parsed by remember { mutableStateOf<HunterCardLog.Log?>(null) }
    var evidence by remember { mutableStateOf<Map<String, Uri>>(emptyMap()) }
    var message by remember { mutableStateOf<String?>(null) }
    val picker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocumentTree()) { uri: Uri? ->
        if (uri != null) {
            runCatching { context.contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION) }
            CardPrefs.writeTree(context, uri)
            chosen = null; parsed = null; tree = uri; usePulled = false
        }
    }
    LaunchedEffect(pull) {
        if (pull is CardPull.State.Done) { usePulled = true; chosen = null; parsed = null; reloads++ }
    }
    LaunchedEffect(tree, usePulled, reloads) {
        if (usePulled) {
            val found = withContext(Dispatchers.IO) { runCatching { CardFiles.localLogs(pulledDir) } }
            logs = found.getOrDefault(emptyList())
            message = if (logs.isEmpty()) "Nothing has been pulled from the Hunter yet." else null
            return@LaunchedEffect
        }
        val t = tree ?: return@LaunchedEffect
        val found = withContext(Dispatchers.IO) { runCatching { CardFiles.logs(context, t) } }
        found.onSuccess { logs = it; message = if (it.isEmpty()) "This folder holds no hunter_NNNN.log files. Pick the card's top folder." else null }
            .onFailure { logs = emptyList(); message = "LayerScope cannot read this folder any more. Pick it again." }
    }
    LaunchedEffect(chosen) {
        val c = chosen ?: return@LaunchedEffect
        val t = tree
        val result = withContext(Dispatchers.IO) {
            runCatching {
                HunterCardLog.parse(CardFiles.readText(context, c.uri)) to
                    (if (usePulled || t == null) CardFiles.localEvidence(pulledDir, c.name) else CardFiles.evidence(context, t, c.name))
            }
        }
        result.onSuccess { (log, ev) -> parsed = log; evidence = ev; message = null }
            .onFailure { parsed = null; message = "LayerScope could not read ${c.name}." }
    }

    LazyColumn(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        item {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                SciFiOutline("‹ BACK", enabled = true) { if (chosen != null) { chosen = null; parsed = null } else onBack() }
                Text("HUNTER CARD", color = LtColors.ConsoleEdge, fontWeight = FontWeight.Bold, fontSize = 16.sp, letterSpacing = 1.sp, style = glowText())
            }
        }
        item {
            Text("Pull the card from the connected Hunter over Bluetooth, or pick the card through a USB card reader " +
                "or a folder it was copied to. Nothing on the card is changed.", color = LtColors.Grey, fontSize = 12.sp)
        }
        item {
            val pulling = pull is CardPull.State.Listing || pull is CardPull.State.Pulling
            // A Hunter without a card: no button, one plain sentence (2026-11-05).
            if (offer != CardPull.Offer.NoCard) {
                SciFiPrimary("PULL FROM THE HUNTER", enabled = offer == CardPull.Offer.Card && !pulling, large = true) {
                    LinkForegroundService.active?.pullCard(pulledDir)
                }
            }
            val status = when (val p = pull) {
                is CardPull.State.Idle -> when (offer) {
                    CardPull.Offer.NoHunter -> "Connect a Hunter with firmware build 93 or later to pull its card over Bluetooth."
                    CardPull.Offer.NoCard -> CardPull.NO_CARD_SENTENCE
                    else -> null
                }
                is CardPull.State.Listing -> p.note
                is CardPull.State.Pulling -> "Pulling file ${p.fileIndex} of ${p.files}: ${p.name}, %,d of %,d bytes.".format(p.bytes, p.size)
                is CardPull.State.Done -> "Pulled ${p.pulled} files in %.1f s; ${p.upToDate} were already up to date.".format(p.elapsedMs / 1000.0) +
                    (if (p.failed.isNotEmpty()) " ${p.failed.size} failed: ${p.failed.first()}" else "")
                is CardPull.State.Failed -> p.message
            }
            status?.let { Text(it, color = if (pull is CardPull.State.Failed) LtColors.Amber else LtColors.Cyan, fontSize = 12.sp) }
        }
        item {
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                SciFiOutline(if (tree == null) "PICK THE HUNTER'S CARD" else "PICK ANOTHER FOLDER", enabled = true) { runCatching { picker.launch(null) } }
                if (pulledDir.exists() && !usePulled) SciFiOutline("SHOW PULLED FILES", enabled = true) { usePulled = true; chosen = null; parsed = null }
            }
        }
        message?.let { m -> item { Text(m, color = LtColors.Amber, fontSize = 12.sp) } }
        val log = parsed
        if (chosen == null) {
            items(logs, key = { it.name }) { f ->
                Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(2.dp)) {
                    SciFiOutline(f.name.uppercase(), enabled = true) { chosen = f }
                    Text("%,d bytes.".format(f.size), color = LtColors.Grey, fontSize = 12.sp, fontFamily = ConsoleFonts.mono)
                }
            }
        } else if (log != null) {
            item {
                val saved = log.detections.count { it.evidencePath != null }
                Text("${chosen!!.name}: boot ${log.bootCount ?: "?"} (${log.resetReason ?: "reset reason not logged"}), " +
                    "${log.detections.size} detections, $saved with saved frames, ${log.links.size} link changes." +
                    (if (log.unreadLines > 0) " ${log.unreadLines} lines were not records LayerScope reads." else ""),
                    color = LtColors.Ivory, fontSize = 12.sp)
            }
            items(log.detections, key = { it.eventId }) { d ->
                Column(Modifier.fillMaxWidth().padding(vertical = 2.dp), verticalArrangement = Arrangement.spacedBy(2.dp)) {
                    Text("#${d.eventId}  ${d.detector}  ${d.confidence.uppercase()}  ${d.source.uppercase()}  ${d.sourceId}",
                        color = LtColors.Green, fontSize = 12.sp, fontFamily = ConsoleFonts.mono, fontWeight = FontWeight.Bold)
                    Text("${d.detail}. Seen ${d.count} times, first ${HunterCardLog.sinceBoot(d.firstMs)}, last ${HunterCardLog.sinceBoot(d.lastMs)}" +
                        (d.rssi?.let { ", signal $it dBm" } ?: "") + (d.channel?.takeIf { it > 0 }?.let { ", channel $it" } ?: "") + ".",
                        color = LtColors.Ivory, fontSize = 12.sp)
                    val uri = d.evidencePath?.let { evidence[it] }
                    when {
                        d.evidencePath == null -> Text("No frames were saved for this detection.", color = LtColors.Grey, fontSize = 12.sp)
                        uri == null -> Text("The log names ${d.evidencePath}, but that file is not in the folder.", color = LtColors.Amber, fontSize = 12.sp)
                        else -> {
                            Text("${d.evidenceKept ?: 0} frames saved of ${d.evidenceSeen ?: 0} heard" +
                                (d.evidenceDropped?.takeIf { it > 0 }?.let { ", $it not kept because the window was full" } ?: "") + ".",
                                color = LtColors.Cyan, fontSize = 12.sp)
                            SciFiOutline("OPEN THE FRAMES IN WIRESHARK", enabled = true) {
                                com.projectrevivesolutions.layertime.controller.flow.TriageSession.focusFrames.value = null
                                PcapImportService.importFile(context, uri)
                                onOpenWireshark()
                            }
                        }
                    }
                }
            }
        }
    }
}

/** A file on the card: its display name, its document URI, and its size. */
data class CardFile(val name: String, val uri: Uri, val size: Long)

/** The folder grant, kept so the operator picks the card once. */
internal object CardPrefs {
    private const val PREFS = "layertime_card"
    fun readTree(ctx: Context): Uri? = ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE).getString("tree", null)?.let(Uri::parse)
    fun writeTree(ctx: Context, uri: Uri) { ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit().putString("tree", uri.toString()).apply() }
}

/** Storage Access Framework reads under the picked folder. */
internal object CardFiles {
    private data class Child(val id: String, val name: String, val mime: String, val size: Long)

    private fun children(ctx: Context, tree: Uri, parentId: String): List<Child> {
        val uri = DocumentsContract.buildChildDocumentsUriUsingTree(tree, parentId)
        val cols = arrayOf(DocumentsContract.Document.COLUMN_DOCUMENT_ID, DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_MIME_TYPE, DocumentsContract.Document.COLUMN_SIZE)
        val out = mutableListOf<Child>()
        ctx.contentResolver.query(uri, cols, null, null, null)?.use { c ->
            while (c.moveToNext()) out += Child(c.getString(0), c.getString(1) ?: "", c.getString(2) ?: "", if (c.isNull(3)) 0 else c.getLong(3))
        }
        return out
    }

    /** The log files at the folder's top, newest serial first. */
    fun logs(ctx: Context, tree: Uri): List<CardFile> =
        children(ctx, tree, DocumentsContract.getTreeDocumentId(tree))
            .filter { HunterCardLog.logSerial(it.name) != null }
            .sortedByDescending { HunterCardLog.logSerial(it.name) }
            .map { CardFile(it.name, DocumentsContract.buildDocumentUriUsingTree(tree, it.id), it.size) }

    fun readText(ctx: Context, uri: Uri): String =
        ctx.contentResolver.openInputStream(uri)?.use { it.readBytes().toString(Charsets.UTF_8) } ?: error("no stream")

    /** The pulled logs in this app's storage, newest serial first. */
    fun localLogs(dir: java.io.File): List<CardFile> =
        (dir.listFiles() ?: emptyArray()).filter { it.isFile && HunterCardLog.logSerial(it.name) != null }
            .sortedByDescending { HunterCardLog.logSerial(it.name) }
            .map { CardFile(it.name, Uri.fromFile(it), it.length()) }

    /** The pulled pcaps for a log, by their path as the log names them. */
    fun localEvidence(dir: java.io.File, logName: String): Map<String, Uri> {
        val folder = HunterCardLog.evidenceFolder(logName) ?: return emptyMap()
        return (java.io.File(dir, folder).listFiles() ?: emptyArray()).filter { it.isFile && it.name.endsWith(".pcap") }
            .associate { "$folder/${it.name}" to Uri.fromFile(it) }
    }

    /** The pcaps in the log's evidence folder, by their path as the log names them ("ev_0007/event_00042.pcap"). */
    fun evidence(ctx: Context, tree: Uri, logName: String): Map<String, Uri> {
        val folder = HunterCardLog.evidenceFolder(logName) ?: return emptyMap()
        val dir = children(ctx, tree, DocumentsContract.getTreeDocumentId(tree))
            .firstOrNull { it.name.equals(folder, ignoreCase = true) && it.mime == DocumentsContract.Document.MIME_TYPE_DIR }
            ?: return emptyMap()
        return children(ctx, tree, dir.id).filter { it.name.endsWith(".pcap", ignoreCase = true) }
            .associate { "$folder/${it.name}" to DocumentsContract.buildDocumentUriUsingTree(tree, it.id) }
    }
}
