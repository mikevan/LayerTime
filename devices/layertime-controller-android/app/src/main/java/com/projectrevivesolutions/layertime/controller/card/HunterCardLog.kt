package com.projectrevivesolutions.layertime.controller.card

/**
 * Reads the Hunter's SD card log (Michael, 2026-11-04), pure and host-tested.
 *
 * The format is the Hunter's own (devices/lilygo-layerwand/src/SdLogLogic.h):
 * a header line, then one CSV line per record with the columns
 * boot,uptime_ms,record,event_id,detector,confidence,source,source_id,detail,rssi,channel,band,count.
 * Text fields are quoted; a quote is doubled, and a backslash or control byte is
 * written as \\ or \xNN. Records: boot, event, repeat, evidence, link, mode.
 *
 * The evidence windows saved during layerwand_NNNN.log are in ev_NNNN beside it,
 * one pcap per detection (event_EEEEE.pcap). An evidence line names its file.
 */
object HunterCardLog {
    const val HEADER = "boot,uptime_ms,record,event_id,detector,confidence,source,source_id,detail,rssi,channel,band,count"

    data class Detection(
        val eventId: Long,
        val detector: String,
        val confidence: String,
        val source: String,
        val sourceId: String,
        val detail: String,
        val firstMs: Long,
        val lastMs: Long,
        val rssi: Int?,
        val channel: Int?,
        val band: String,
        val count: Long,
        /** The pcap's path beside the log ("ev_0007/event_00042.pcap"), once a window was saved. */
        val evidencePath: String? = null,
        val evidenceKept: Int? = null,
        val evidenceSeen: Int? = null,
        val evidenceDropped: Int? = null,
    )

    data class Link(val uptimeMs: Long, val connected: Boolean, val peer: String)

    data class Log(
        val bootCount: Long?,
        val resetReason: String?,
        val detections: List<Detection>,
        val links: List<Link>,
        val modes: List<Pair<Long, String>>,
        /** Lines that were not a record this reader knows (counted, never silently dropped). */
        val unreadLines: Int,
    )

    /** The serial in "layerwand_NNNN.log" (any case), or null. */
    fun logSerial(name: String): Int? =
        Regex("^layerwand_(\\d{4})\\.log$", RegexOption.IGNORE_CASE).find(name.trim())?.groupValues?.get(1)?.toIntOrNull()

    /** The evidence folder for a log file name ("layerwand_0007.log" to "ev_0007"), or null. */
    fun evidenceFolder(logName: String): String? = logSerial(logName)?.let { "ev_%04d".format(it) }

    /** One CSV line's fields, quotes and escapes undone. */
    fun fields(line: String): List<String> {
        val out = mutableListOf<String>()
        val cur = StringBuilder()
        var i = 0
        var quoted = false
        while (i < line.length) {
            val c = line[i]
            when {
                quoted && c == '"' && i + 1 < line.length && line[i + 1] == '"' -> { cur.append('"'); i++ }
                c == '"' -> quoted = !quoted
                quoted && c == '\\' && i + 1 < line.length && line[i + 1] == '\\' -> { cur.append('\\'); i++ }
                quoted && c == '\\' && i + 3 < line.length && line[i + 1] == 'x' -> {
                    val v = line.substring(i + 2, i + 4).toIntOrNull(16)
                    if (v != null) { cur.append(v.toChar()); i += 3 } else cur.append(c)
                }
                !quoted && c == ',' -> { out += cur.toString(); cur.clear() }
                else -> cur.append(c)
            }
            i++
        }
        out += cur.toString()
        return out
    }

    fun parse(text: String): Log {
        var boot: Long? = null
        var reason: String? = null
        val byId = linkedMapOf<Long, Detection>()
        val links = mutableListOf<Link>()
        val modes = mutableListOf<Pair<Long, String>>()
        var unread = 0
        for (raw in text.lineSequence()) {
            val line = raw.trimEnd('\r')
            if (line.isBlank() || line == HEADER) continue
            val f = fields(line)
            if (f.size != 13) { unread++; continue }
            val uptime = f[1].toLongOrNull()
            val id = f[3].toLongOrNull()
            when (f[2]) {
                "boot" -> { boot = f[0].toLongOrNull(); reason = f[8] }
                "event" -> if (id != null && uptime != null) {
                    byId[id] = Detection(id, f[4], f[5], f[6], f[7], f[8], uptime, uptime,
                        f[9].toIntOrNull(), f[10].toIntOrNull(), f[11], f[12].toLongOrNull() ?: 1)
                } else unread++
                "repeat" -> if (id != null && uptime != null) {
                    val old = byId[id]
                    byId[id] = old?.copy(lastMs = uptime, rssi = f[9].toIntOrNull() ?: old.rssi,
                        channel = f[10].toIntOrNull() ?: old.channel, count = f[12].toLongOrNull() ?: old.count,
                        confidence = f[5].ifBlank { old.confidence })
                        // A repeat whose first sighting is in an earlier file: keep it as its own row.
                        ?: Detection(id, f[4], f[5], f[6], f[7], f[8], uptime, uptime,
                            f[9].toIntOrNull(), f[10].toIntOrNull(), f[11], f[12].toLongOrNull() ?: 1)
                } else unread++
                "evidence" -> if (id != null) {
                    val parts = f[8].split(' ')
                    val path = parts.firstOrNull()?.takeIf { it.endsWith(".pcap") }
                    fun num(key: String) = parts.firstOrNull { it.startsWith("$key=") }?.substringAfter('=')?.toIntOrNull()
                    val old = byId[id]
                    if (old != null && path != null) byId[id] = old.copy(evidencePath = path,
                        evidenceKept = num("kept"), evidenceSeen = num("seen"), evidenceDropped = num("dropped"))
                    else unread++
                } else unread++
                "link" -> if (uptime != null) links += Link(uptime, f[8] == "connected", f[7]) else unread++
                "mode" -> if (uptime != null) modes += uptime to f[8] else unread++
                else -> unread++
            }
        }
        return Log(boot, reason, byId.values.toList(), links, modes, unread)
    }

    /** "1 min 21 s after boot" style, for a list row. */
    fun sinceBoot(ms: Long): String {
        val s = ms / 1000
        return when {
            s < 60 -> "$s s after boot"
            s < 3600 -> "${s / 60} min ${s % 60} s after boot"
            else -> "${s / 3600} h ${(s % 3600) / 60} min after boot"
        }
    }
}
