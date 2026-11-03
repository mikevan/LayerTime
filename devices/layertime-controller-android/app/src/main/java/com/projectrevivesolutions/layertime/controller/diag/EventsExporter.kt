package com.projectrevivesolutions.layertime.controller.diag

import android.content.Context
import android.content.Intent
import android.net.Uri
import androidx.core.content.FileProvider
import com.projectrevivesolutions.layertime.controller.events.EventsExport
import com.projectrevivesolutions.layertime.controller.model.EventRow
import com.projectrevivesolutions.layertime.controller.state.ControllerRepository
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Writes the current event list as analysis-ready CSV and JSON, triaged by
 * [EventsExport] / EventTriage, and shares the CSV. Two destinations:
 *   - the app external files dir (Android/data/<pkg>/files/exports): pullable
 *     over adb or a file manager with no run-as, for off-phone analysis.
 *   - the app internal files dir (files/exports): shared via the existing
 *     FileProvider (file_paths.xml files-path "exports").
 *
 * [autosaveNow] keeps a single rolling pair (events-latest.csv/json) current in
 * the external dir so the session survives the app being killed and can be
 * pulled at any time. It is best-effort; a failure never touches the link.
 */
object EventsExporter {

    data class Written(val shareCsv: File, val externalCsv: File?, val externalJson: File?)

    private fun stamp() = SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US).format(Date())

    private fun exportsDir(parent: File?): File? =
        parent?.let { File(it, "exports").apply { mkdirs() } }

    /** Writes a timestamped snapshot to both dirs; returns the internal CSV to share, or null on failure. */
    fun writeSnapshot(context: Context): Written? = runCatching {
        val rows = ControllerRepository.events.value
        val csv = EventsExport.csv(rows)
        val json = EventsExport.json(rows)
        val s = stamp()

        val shareDir = exportsDir(context.filesDir)!!
        val shareCsv = File(shareDir, "layertime-events-$s.csv").apply { writeText(csv) }
        File(shareDir, "layertime-events-$s.json").writeText(json)

        val extDir = exportsDir(context.getExternalFilesDir(null))
        val extCsv = extDir?.let { File(it, "layertime-events-$s.csv").apply { writeText(csv) } }
        val extJson = extDir?.let { File(it, "layertime-events-$s.json").apply { writeText(json) } }
        Written(shareCsv, extCsv, extJson)
    }.getOrNull()

    fun shareIntent(context: Context, csv: File): Intent {
        val uri: Uri = FileProvider.getUriForFile(context, "${context.packageName}.fileprovider", csv)
        return Intent(Intent.ACTION_SEND).apply {
            type = "text/csv"
            putExtra(Intent.EXTRA_STREAM, uri)
            putExtra(Intent.EXTRA_SUBJECT, "LayerScope events")
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
    }

    /**
     * Best-effort rolling write of the current list to the external dir, overwriting
     * the previous pair. Call from a debounced collector so the final state lands.
     */
    fun autosaveNow(context: Context, rows: List<EventRow>, nowMs: Long = System.currentTimeMillis()) {
        runCatching {
            val extDir = exportsDir(context.getExternalFilesDir(null)) ?: return
            File(extDir, "layertime-events-latest.csv").writeText(EventsExport.csv(rows))
            File(extDir, "layertime-events-latest.json").writeText(EventsExport.json(rows, nowMs))
        }
    }
}
