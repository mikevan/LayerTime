package com.projectrevivesolutions.layertime.controller.diag

import android.content.Context
import android.content.Intent
import android.net.Uri
import androidx.core.content.FileProvider
import com.projectrevivesolutions.layertime.controller.link.LinkConstants
import com.projectrevivesolutions.layertime.controller.model.EventRow
import com.projectrevivesolutions.layertime.controller.model.LogEntry
import com.projectrevivesolutions.layertime.controller.state.ControllerRepository
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Writes a diagnostic bundle (machine-readable JSON + a human-readable text
 * summary) and returns a share intent. Captures exactly what the milestone
 * asks to export: the device/BLE report, the connection timeline, the event
 * log, and the screen-off reception test result.
 *
 * JSON is built by hand to avoid adding a serialization dependency; all
 * dynamic strings are escaped, and RF-sourced text is treated as untrusted.
 */
object Exporter {

    private val tsFmt = SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss.SSSZ", Locale.US)
    private fun ts(ms: Long) = tsFmt.format(Date(ms))

    /** Returns the JSON file written, or null on failure. */
    fun writeBundle(context: Context): File? = runCatching {
        val dir = File(context.filesDir, "exports").apply { mkdirs() }
        val stamp = SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US).format(Date())
        val json = File(dir, "layertime-diag-$stamp.json")
        json.writeText(buildJson())
        File(dir, "layertime-diag-$stamp.txt").writeText(buildText())
        json
    }.getOrNull()

    fun shareIntent(context: Context, file: File): Intent {
        val authority = "${context.packageName}.fileprovider"
        val uri: Uri = FileProvider.getUriForFile(context, authority, file)
        return Intent(Intent.ACTION_SEND).apply {
            type = "application/json"
            putExtra(Intent.EXTRA_STREAM, uri)
            putExtra(Intent.EXTRA_SUBJECT, "LayerTime controller diagnostics")
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
    }

    private fun esc(s: String?): String {
        if (s == null) return "null"
        val sb = StringBuilder("\"")
        for (c in s) when (c) {
            '"' -> sb.append("\\\""); '\\' -> sb.append("\\\\")
            '\n' -> sb.append("\\n"); '\r' -> sb.append("\\r"); '\t' -> sb.append("\\t")
            else -> if (c < ' ') sb.append("\\u%04x".format(c.code)) else sb.append(c)
        }
        return sb.append("\"").toString()
    }

    private fun buildJson(): String {
        val repo = ControllerRepository
        val d = repo.deviceReport
        val server = repo.server.value
        val status = repo.status.value
        val bt = repo.background.value
        val sb = StringBuilder()
        sb.append("{\n")
        sb.append("  \"schema\": \"layertime-controller-diagnostic/1\",\n")
        sb.append("  \"linkContract\": \"0.1\",\n")
        sb.append("  \"generatedAt\": ${esc(ts(System.currentTimeMillis()))},\n")

        sb.append("  \"device\": ")
        if (d == null) sb.append("null") else sb.append(
            "{" +
                "\"manufacturer\": ${esc(d.manufacturer)}, \"model\": ${esc(d.model)}, " +
                "\"device\": ${esc(d.device)}, \"product\": ${esc(d.product)}, " +
                "\"androidRelease\": ${esc(d.androidRelease)}, \"sdkInt\": ${d.sdkInt}, " +
                "\"bleSupported\": ${d.bleSupported}, \"bluetoothOn\": ${d.bluetoothOn}, " +
                "\"le2mPhy\": ${d.le2mPhy}, \"leCodedPhy\": ${d.leCodedPhy}, " +
                "\"leExtendedAdvertising\": ${d.leExtendedAdvertising}, " +
                "\"multipleAdvertisement\": ${d.multipleAdvertisement}, " +
                "\"appVersionName\": ${esc(d.appVersionName)}, \"appVersionCode\": ${d.appVersionCode}" +
                "}"
        )
        sb.append(",\n")

        sb.append("  \"server\": ")
        if (server == null) sb.append("null") else sb.append(
            "{\"serverMajor\": ${server.serverMajor}, \"serverMinor\": ${server.serverMinor}, " +
                "\"sessionId\": ${server.sessionId}, \"capabilities\": ${server.capabilities}, " +
                "\"capabilityNames\": ${esc(LinkConstants.Capability.describe(server.capabilities).joinToString(","))}, " +
                "\"maxFrame\": ${server.maxFrame}}"
        )
        sb.append(",\n")

        sb.append("  \"lastStatus\": ")
        if (status == null) sb.append("null") else sb.append(
            "{\"linkVersion\": ${status.linkVersion}, \"sessionId\": ${status.sessionId}, " +
                "\"changeSeq\": ${status.changeSeq}, \"flags\": ${status.flags}, " +
                "\"monitoring\": ${status.monitoring}, \"eventCount\": ${status.eventCount}, " +
                "\"active\": ${status.active}, \"activeName\": ${esc(LinkConstants.ReconTarget.name(status.active))}, " +
                "\"heartbeat\": ${status.heartbeat}}"
        )
        sb.append(",\n")

        sb.append("  \"backgroundTest\": ")
        if (bt == null) sb.append("null") else sb.append(
            "{\"running\": ${bt.running}, \"startedAt\": ${esc(ts(bt.startedMillis))}, " +
                "\"endedAt\": ${if (bt.endedMillis != null) esc(ts(bt.endedMillis)) else "null"}, " +
                "\"eventsAtStart\": ${bt.eventsAtStart}, \"eventsReceivedDuring\": ${bt.eventsReceivedDuring}, " +
                "\"disconnectsDuring\": ${bt.disconnectsDuring}, \"note\": ${esc(bt.note)}}"
        )
        sb.append(",\n")

        sb.append("  \"events\": [\n")
        val events = repo.events.value
        events.forEachIndexed { i, e -> sb.append("    ").append(eventJson(e)).append(if (i < events.lastIndex) ",\n" else "\n") }
        sb.append("  ],\n")

        sb.append("  \"log\": [\n")
        val log = repo.log.value
        log.forEachIndexed { i, l ->
            sb.append("    {\"t\": ${esc(ts(l.epochMillis))}, \"category\": ${esc(l.category.name)}, \"message\": ${esc(l.message)}}")
            sb.append(if (i < log.lastIndex) ",\n" else "\n")
        }
        sb.append("  ]\n")
        sb.append("}\n")
        return sb.toString()
    }

    private fun eventJson(e: EventRow): String =
        "{\"sessionId\": ${e.sessionId}, \"eventId\": ${e.eventId}, " +
            "\"detector\": ${e.detector}, \"detectorName\": ${esc(e.detectorName)}, " +
            "\"confidence\": ${e.confidence}, \"confidenceName\": ${esc(LinkConstants.Confidence.name(e.confidence))}, " +
            "\"sourceKind\": ${e.sourceKind}, \"sourceKindName\": ${esc(LinkConstants.SourceKind.name(e.sourceKind))}, " +
            "\"band\": ${e.band}, \"bandName\": ${esc(LinkConstants.Band.name(e.band))}, " +
            "\"channel\": ${e.channel}, \"rssiDbm\": ${e.rssi}, \"count\": ${e.count}, " +
            "\"ageSeconds\": ${e.ageSeconds}, " +
            "\"sourceId\": ${esc(e.sourceId)}, \"detail\": ${esc(e.detail)}, " +
            "\"firstSeenAt\": ${esc(ts(e.firstSeenMillis))}, \"lastUpdatedAt\": ${esc(ts(e.lastUpdatedMillis))}}"

    private fun buildText(): String {
        val repo = ControllerRepository
        val d = repo.deviceReport
        val sb = StringBuilder()
        sb.appendLine("LayerTime Controller — diagnostic export")
        sb.appendLine("Generated: ${ts(System.currentTimeMillis())}")
        sb.appendLine("Link contract: 0.1")
        sb.appendLine()
        sb.appendLine("DEVICE")
        if (d != null) {
            sb.appendLine("  ${d.manufacturer} ${d.model} (device=${d.device}, product=${d.product})")
            sb.appendLine("  Android ${d.androidRelease}, API ${d.sdkInt}")
            sb.appendLine("  BLE supported=${d.bleSupported}, Bluetooth on=${d.bluetoothOn}")
            sb.appendLine("  LE 2M PHY=${d.le2mPhy}, LE coded PHY=${d.leCodedPhy}, extended advertising=${d.leExtendedAdvertising}")
            sb.appendLine("  App ${d.appVersionName} (${d.appVersionCode})")
        }
        val bt = repo.background.value
        if (bt != null) {
            sb.appendLine()
            sb.appendLine("SCREEN-OFF / BACKGROUND RECEPTION TEST")
            sb.appendLine("  started ${ts(bt.startedMillis)}")
            sb.appendLine("  ended ${bt.endedMillis?.let { ts(it) } ?: "(running)"}")
            sb.appendLine("  events received during window: ${bt.eventsReceivedDuring}")
            sb.appendLine("  disconnects during window: ${bt.disconnectsDuring}")
            sb.appendLine("  note: ${bt.note}")
        }
        sb.appendLine()
        sb.appendLine("EVENTS (${repo.events.value.size})")
        for (e in repo.events.value) {
            sb.appendLine("  #${e.eventId} ${e.detectorName} ${LinkConstants.SourceKind.name(e.sourceKind)} " +
                "${LinkConstants.Band.name(e.band)} ch${e.channel} ${e.rssi}dBm x${e.count} age${e.ageSeconds}s " +
                "conf=${LinkConstants.Confidence.name(e.confidence)}" +
                (e.sourceId?.let { " id=\"$it\"" } ?: "") + (e.detail?.let { " detail=\"$it\"" } ?: ""))
        }
        sb.appendLine()
        sb.appendLine("CONNECTION / EVENT LOG (${repo.log.value.size} entries)")
        for (l in repo.log.value) sb.appendLine("  ${ts(l.epochMillis)} [${l.category}] ${l.message}")
        return sb.toString()
    }
}
