package com.projectrevivesolutions.layertime.controller.events

import com.projectrevivesolutions.layertime.controller.model.EventRow
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.TimeZone

/**
 * Serializes the grouped event list (one row per MAC) to analysis-ready CSV and
 * JSON, each row tagged with its [EventTriage] verdict. Pure, no Android types,
 * host-tested (EventsExportTest). Timestamps are ISO-8601 UTC.
 *
 * All RF-sourced strings (sourceId, detail, detectorName) are treated as
 * untrusted: CSV fields are quoted per RFC 4180 and JSON strings are escaped.
 */
object EventsExport {

    val HEADER = listOf(
        "session", "eventId", "firstSeenUtc", "lastSeenUtc", "ageSeconds",
        "sourceKind", "sourceId", "detector", "detectorName", "confidence", "confidenceName",
        "band", "channel", "rssiDbm", "count", "hunterRecords", "dwellSeconds", "detail", "triage", "triageReason",
    )

    fun csv(rows: List<EventRow>): String {
        val sb = StringBuilder()
        sb.append(HEADER.joinToString(",")).append("\r\n")
        for (e in rows) {
            val v = EventTriage.classify(e)
            val cells = listOf(
                e.sessionId.toString(), e.eventId.toString(), iso(e.firstSeenMillis), iso(e.lastUpdatedMillis),
                e.ageSeconds.toString(), sourceKindName(e.sourceKind), e.sourceId ?: "",
                e.detector.toString(), e.detectorName, e.confidence.toString(), confName(e.confidence),
                bandName(e.band), e.channel.toString(), e.rssi.toString(), e.count.toString(),
                e.mergedEventIds.size.toString(), (EventTriage.dwellSpanMillis(e) / 1000).toString(),
                e.detail ?: "", v.triage.name, v.reason,
            )
            sb.append(cells.joinToString(",") { csvField(it) }).append("\r\n")
        }
        return sb.toString()
    }

    fun json(rows: List<EventRow>, nowMs: Long = System.currentTimeMillis()): String {
        val c = EventTriage.counts(rows)
        val sb = StringBuilder()
        sb.append("{\n")
        sb.append("  \"schema\": \"layertime-events/1\",\n")
        sb.append("  \"generatedAtUtc\": ").append(esc(iso(nowMs))).append(",\n")
        sb.append("  \"total\": ${rows.size},\n")
        sb.append("  \"counts\": {\"track\": ${c[EventTriage.Triage.TRACK] ?: 0}, ")
        sb.append("\"keep\": ${c[EventTriage.Triage.KEEP] ?: 0}, \"noise\": ${c[EventTriage.Triage.NOISE] ?: 0}},\n")
        sb.append("  \"events\": [\n")
        rows.forEachIndexed { i, e ->
            val v = EventTriage.classify(e)
            sb.append("    {")
            sb.append("\"session\": ${e.sessionId}, \"eventId\": ${e.eventId}, ")
            sb.append("\"firstSeenUtc\": ${esc(iso(e.firstSeenMillis))}, \"lastSeenUtc\": ${esc(iso(e.lastUpdatedMillis))}, ")
            sb.append("\"ageSeconds\": ${e.ageSeconds}, ")
            sb.append("\"sourceKind\": ${e.sourceKind}, \"sourceKindName\": ${esc(sourceKindName(e.sourceKind))}, ")
            sb.append("\"sourceId\": ${esc(e.sourceId)}, ")
            sb.append("\"detector\": ${e.detector}, \"detectorName\": ${esc(e.detectorName)}, ")
            sb.append("\"confidence\": ${e.confidence}, \"confidenceName\": ${esc(confName(e.confidence))}, ")
            sb.append("\"band\": ${e.band}, \"bandName\": ${esc(bandName(e.band))}, ")
            sb.append("\"channel\": ${e.channel}, \"rssiDbm\": ${e.rssi}, \"count\": ${e.count}, ")
            sb.append("\"hunterRecords\": ${e.mergedEventIds.size}, \"dwellSeconds\": ${EventTriage.dwellSpanMillis(e) / 1000}, ")
            sb.append("\"detail\": ${esc(e.detail)}, ")
            sb.append("\"triage\": ${esc(v.triage.name)}, \"triageReason\": ${esc(v.reason)}}")
            sb.append(if (i < rows.lastIndex) ",\n" else "\n")
        }
        sb.append("  ]\n}\n")
        return sb.toString()
    }

    private fun iso(ms: Long): String {
        val f = SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss.SSS'Z'", Locale.US)
        f.timeZone = TimeZone.getTimeZone("UTC")
        return f.format(Date(ms))
    }

    private fun csvField(s: String): String {
        val needQuote = s.any { it == ',' || it == '"' || it == '\n' || it == '\r' }
        val body = s.replace("\"", "\"\"")
        return if (needQuote) "\"$body\"" else body
    }

    private fun esc(s: String?): String {
        if (s == null) return "null"
        val sb = StringBuilder("\"")
        for (ch in s) when (ch) {
            '"' -> sb.append("\\\""); '\\' -> sb.append("\\\\")
            '\n' -> sb.append("\\n"); '\r' -> sb.append("\\r"); '\t' -> sb.append("\\t")
            else -> if (ch < ' ') sb.append("\\u%04x".format(ch.code)) else sb.append(ch)
        }
        return sb.append("\"").toString()
    }

    private fun confName(v: Int) = when (v) { 0 -> "LOW"; 1 -> "MED"; 2 -> "HIGH"; else -> "?($v)" }
    private fun sourceKindName(v: Int) = when (v) { 0 -> "Unknown"; 1 -> "Wi-Fi"; 2 -> "BLE"; 3 -> "802.15.4"; else -> "Unknown($v)" }
    private fun bandName(v: Int) = when (v) { 0 -> "Unknown"; 1 -> "2.4 GHz"; 2 -> "5 GHz"; else -> "Unknown($v)" }
}
