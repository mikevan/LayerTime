package com.projectrevivesolutions.layertime.controller.tools

import com.projectrevivesolutions.layertime.controller.link.Frame
import com.projectrevivesolutions.layertime.controller.link.LinkCodec
import com.projectrevivesolutions.layertime.controller.link.LinkConstants
import java.io.File

/**
 * Host conformance test for the Android LayerTime Link binding.
 *
 * Reads the canonical byte-exact vectors in contracts/vectors/link_frames.json
 * and checks that this binding:
 *   - encodes every request case to exactly the listed bytes,
 *   - decodes every status snapshot to exactly the listed fields,
 *   - decodes every reply frame to exactly the listed fields,
 *   - reassembles a multi-frame GET_CHANGED run and a GET_TEXT run correctly.
 *
 * No Android SDK and no third-party libraries are required: it compiles the
 * app's link/ package (pure Kotlin) plus MiniJson and runs on a host JVM. It
 * exits non-zero on any failure, so it works as a CI gate.
 *
 * Usage: LinkConformanceKt [path-to-link_frames.json]
 *        default path: contracts/vectors/link_frames.json
 */

private var passed = 0
private var failed = 0

private fun check(name: String, cond: Boolean, detail: () -> String = { "" }) {
    if (cond) { passed++ } else { failed++; println("  FAIL  $name  ${detail()}") }
}

private fun eqBytes(name: String, got: ByteArray, wantHex: String) {
    val want = LinkCodec.fromHex(wantHex)
    check(name, got.contentEquals(want)) { "got ${LinkCodec.toHex(got)} want ${wantHex.uppercase()}" }
}

fun main(args: Array<String>) {
    val path = args.getOrNull(0) ?: "contracts/vectors/link_frames.json"
    val file = File(path)
    if (!file.exists()) { System.err.println("vectors not found: ${file.absolutePath}"); kotlin.system.exitProcess(2) }
    val root = MiniJson.parse(file.readText()).obj()

    println("LayerTime Link conformance (Android binding) against ${file.path}")
    println("contract ${root.str("contractVersion")}, link ${root.str("linkVersion")}")

    // ---- requests: encode to exact bytes ----
    for (c in root["requests"].arr()) {
        val m = c.obj(); val f = m["fields"].obj(); val name = "req/${m.str("name")}"
        val got = when (m.str("op")) {
            "HELLO" -> LinkCodec.hello(f.int("reqId"), f.int("clientMajor"), f.int("clientMinor"))
            "PING" -> LinkCodec.ping(f.int("reqId"), f.long("token"))
            "GET_CHANGED" -> LinkCodec.getChanged(f.int("reqId"), f.long("sinceChangeSeq"))
            "GET_TEXT" -> LinkCodec.getText(f.int("reqId"), f.long("eventId"), f.int("field"))
            "COMMAND" -> encodeCommand(f)
            else -> { check(name, false) { "unknown op" }; continue }
        }
        eqBytes(name, got, m.str("bytes"))
    }

    // ---- status snapshots: decode to exact fields ----
    for (c in root["status"].arr()) {
        val m = c.obj(); val f = m["fields"].obj(); val name = "status/${m.str("name")}"
        val s = LinkCodec.decodeStatus(LinkCodec.fromHex(m.str("bytes")))
        if (s == null) { check(name, false) { "decodeStatus returned null" }; continue }
        check(name, s.linkVersion == f.int("linkVersion") && s.sessionId == f.int("sessionId") &&
            s.changeSeq == f.long("changeSeq") && s.flags == f.int("flags") &&
            s.selected == f.int("selected") && s.active == f.int("active") &&
            s.eventCount == f.int("eventCount") && s.lastAlertEventId == f.long("lastAlertEventId") &&
            s.heartbeat == f.int("heartbeat") && s.schedule == f.int("schedule") &&
            s.nextReportS == f.int("nextReportS")) { "decoded $s" }
    }

    // ---- replies: decode to exact fields ----
    for (c in root["replies"].arr()) {
        val m = c.obj(); val f = m["fields"].obj(); val name = "reply/${m.str("name")}"
        val fr = LinkCodec.decodeFrame(LinkCodec.fromHex(m.str("bytes")))
        checkReply(name, m.str("type"), fr, f)
    }

    // ---- multi-frame reassembly (client logic) ----
    reassemblyChecks(root)

    println("---------------------------------------------")
    println("PASS=$passed  FAIL=$failed")
    if (failed > 0) kotlin.system.exitProcess(1)
    println("ALL CONFORMANCE CHECKS PASSED")
}

/** Rebuild COMMAND bytes from the public encoders, keyed by commandType. */
private fun encodeCommand(f: Map<String, Any?>): ByteArray {
    val reqId = f.int("reqId")
    return when (f.int("commandType")) {
        LinkConstants.CommandType.RECON_START -> LinkCodec.reconStart(reqId, f.int("argument"))
        LinkConstants.CommandType.RECON_STOP -> LinkCodec.reconStop(reqId)
        LinkConstants.CommandType.RECON_CLEAR_EVENTS -> LinkCodec.reconClearEvents(reqId)
        LinkConstants.CommandType.RECON_ACK_ALERT -> LinkCodec.reconAcknowledgeAlert(reqId)
        LinkConstants.CommandType.SET_SLEEP_MODE -> LinkCodec.setSleepMode(reqId, f.int("argument") == 1)
        LinkConstants.CommandType.SET_EARLY_WARNING -> LinkCodec.setEarlyWarning(reqId, f.int("argument") == 1)
        else -> ByteArray(0)
    }
}

private fun checkReply(name: String, type: String, fr: Frame, f: Map<String, Any?>) {
    when (type) {
        "HELLO_ACK" -> {
            val a = fr as? Frame.HelloAck ?: return check(name, false) { "not HelloAck: $fr" }
            check(name, a.reqId == f.int("reqId") && a.linkStatus == f.int("linkStatus") &&
                a.serverMajor == f.int("serverMajor") && a.serverMinor == f.int("serverMinor") &&
                a.sessionId == f.int("sessionId") && a.capabilities == f.int("capabilities") &&
                a.maxFrame == f.int("maxFrame")) { "$a" }
        }
        "ACK" -> {
            val a = fr as? Frame.Ack ?: return check(name, false) { "not Ack: $fr" }
            check(name, a.reqId == f.int("reqId") && a.linkStatus == f.int("linkStatus") &&
                a.token == f.long("token") && a.heartbeat == f.int("heartbeat")) { "$a" }
        }
        "RESULT" -> {
            val a = fr as? Frame.Result ?: return check(name, false) { "not Result: $fr" }
            check(name, a.reqId == f.int("reqId") && a.linkStatus == f.int("linkStatus") &&
                a.commandType == f.int("commandType") && a.commandResult == f.int("commandResult")) { "$a" }
        }
        "EVENT_SUMMARY" -> {
            val a = fr as? Frame.EventSummary ?: return check(name, false) { "not EventSummary: $fr" }
            check(name, a.reqId == f.int("reqId") && a.linkStatus == f.int("linkStatus") &&
                a.eventId == f.long("eventId") && a.detector == f.int("detector") &&
                a.confidence == f.int("confidence") && a.sourceKind == f.int("sourceKind") &&
                a.band == f.int("band") && a.channel == f.int("channel") && a.rssi == f.int("rssi") &&
                a.count == f.int("count") && a.ageSeconds == f.int("ageSeconds") &&
                a.flags == f.int("flags")) { "$a" }
        }
        "END" -> {
            val a = fr as? Frame.End ?: return check(name, false) { "not End: $fr" }
            check(name, a.reqId == f.int("reqId") && a.linkStatus == f.int("linkStatus") &&
                a.count == f.int("count") && a.gap == f.int("gap") && a.changeSeq == f.long("changeSeq")) { "$a" }
        }
        "TEXT" -> {
            val a = fr as? Frame.Text ?: return check(name, false) { "not Text: $fr" }
            check(name, a.reqId == f.int("reqId") && a.linkStatus == f.int("linkStatus") &&
                a.field == f.int("field") && a.index == f.int("index") && a.total == f.int("total") &&
                a.bytes.size == f.int("length")) { "$a size=${a.bytes.size}" }
        }
        "ERROR" -> {
            val a = fr as? Frame.Error ?: return check(name, false) { "not Error: $fr" }
            check(name, a.reqId == f.int("reqId") && a.linkStatus == f.int("linkStatus")) { "$a" }
        }
        else -> check(name, false) { "unhandled type $type" }
    }
}

/**
 * Prove the client-side reassembly a controller must do: collect EVENT_SUMMARY
 * frames until END (GET_CHANGED), and reassemble TEXT fragments in order
 * (GET_TEXT). Uses the canonical reply bytes from the vectors.
 */
private fun reassemblyChecks(root: Map<String, Any?>) {
    val byName = root["replies"].arr().associate { it.obj().str("name") to it.obj().str("bytes") }

    // GET_CHANGED: two EVENT_SUMMARY then END(count=2, gap=1).
    val summaries = ArrayList<Frame.EventSummary>()
    var end: Frame.End? = null
    for (n in listOf("event_summary_wifi_negative_rssi", "event_summary_ble_saturated", "end_after_two_with_gap")) {
        when (val fr = LinkCodec.decodeFrame(LinkCodec.fromHex(byName.getValue(n)))) {
            is Frame.EventSummary -> summaries.add(fr)
            is Frame.End -> end = fr
            else -> {}
        }
    }
    check("reassembly/get_changed_count", summaries.size == 2) { "collected ${summaries.size}" }
    check("reassembly/get_changed_end_count_matches", end?.count == summaries.size) { "end=${end?.count}" }
    check("reassembly/get_changed_gap_flag", end?.gap == 1) { "gap=${end?.gap}" }
    check("reassembly/get_changed_deauth_detector",
        summaries.firstOrNull()?.detector == LinkConstants.ReconTarget.DEAUTH) { "detector=${summaries.firstOrNull()?.detector}" }

    // GET_TEXT: two TEXT fragments -> "Free Public Wi" + "Fi" = "Free Public WiFi".
    val frags = listOf("text_first_fragment_full", "text_last_fragment_short")
        .map { LinkCodec.decodeFrame(LinkCodec.fromHex(byName.getValue(it))) as Frame.Text }
        .sortedBy { it.index }
    val text = frags.joinToString("") { String(it.bytes, Charsets.UTF_8) }
    check("reassembly/get_text_reassembled", text == "Free Public WiFi") { "got '$text'" }
}
