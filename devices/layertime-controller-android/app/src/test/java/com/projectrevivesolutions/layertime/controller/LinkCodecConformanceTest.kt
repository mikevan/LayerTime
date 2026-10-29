package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.link.Frame
import com.projectrevivesolutions.layertime.controller.link.LinkCodec
import com.projectrevivesolutions.layertime.controller.link.LinkConstants
import com.projectrevivesolutions.layertime.controller.tools.MiniJson
import com.projectrevivesolutions.layertime.controller.tools.arr
import com.projectrevivesolutions.layertime.controller.tools.int
import com.projectrevivesolutions.layertime.controller.tools.long
import com.projectrevivesolutions.layertime.controller.tools.obj
import com.projectrevivesolutions.layertime.controller.tools.str
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

/**
 * Runs the LayerTime Link binding against the canonical byte-exact vectors in
 * contracts/vectors/link_frames.json. This is the same check the host tool in
 * tools/ci performs, wired as a Gradle unit test so CI and local builds gate on
 * it (./gradlew :app:testDebugUnitTest). The binding is wrong if any case fails.
 */
class LinkCodecConformanceTest {

    private fun locateVectors(): File {
        // Walk up from the module working directory to the repo root.
        var dir: File? = File(System.getProperty("user.dir")).absoluteFile
        while (dir != null) {
            val f = File(dir, "contracts/vectors/link_frames.json")
            if (f.exists()) return f
            dir = dir.parentFile
        }
        error("contracts/vectors/link_frames.json not found above ${System.getProperty("user.dir")}")
    }

    private val root by lazy { MiniJson.parse(locateVectors().readText()).obj() }
    private fun hex(s: String) = LinkCodec.fromHex(s)

    @Test fun requests_encode_byte_exact() {
        for (c in root["requests"].arr()) {
            val m = c.obj(); val f = m["fields"].obj(); val name = m.str("name")
            val got = when (m.str("op")) {
                "HELLO" -> LinkCodec.hello(f.int("reqId"), f.int("clientMajor"), f.int("clientMinor"))
                "PING" -> LinkCodec.ping(f.int("reqId"), f.long("token"))
                "GET_CHANGED" -> LinkCodec.getChanged(f.int("reqId"), f.long("sinceChangeSeq"))
                "GET_TEXT" -> LinkCodec.getText(f.int("reqId"), f.long("eventId"), f.int("field"))
                "COMMAND" -> when (f.int("commandType")) {
                    1 -> LinkCodec.reconStart(f.int("reqId"), f.int("argument"))
                    2 -> LinkCodec.reconStop(f.int("reqId"))
                    3 -> LinkCodec.reconClearEvents(f.int("reqId"))
                    4 -> LinkCodec.reconAcknowledgeAlert(f.int("reqId"))
                    11 -> LinkCodec.setSleepMode(f.int("reqId"), f.int("argument") == 1)
                    12 -> LinkCodec.setEarlyWarning(f.int("reqId"), f.int("argument") == 1)
                    else -> error("unhandled commandType in $name")
                }
                else -> error("unhandled op in $name")
            }
            assertEquals("request $name", m.str("bytes").uppercase(), LinkCodec.toHex(got))
        }
    }

    @Test fun status_decodes_exact_fields() {
        for (c in root["status"].arr()) {
            val m = c.obj(); val f = m["fields"].obj(); val name = m.str("name")
            val s = LinkCodec.decodeStatus(hex(m.str("bytes"))) ?: error("null status $name")
            assertEquals("$name linkVersion", f.int("linkVersion"), s.linkVersion)
            assertEquals("$name sessionId", f.int("sessionId"), s.sessionId)
            assertEquals("$name changeSeq", f.long("changeSeq"), s.changeSeq)
            assertEquals("$name flags", f.int("flags"), s.flags)
            assertEquals("$name eventCount", f.int("eventCount"), s.eventCount)
            assertEquals("$name lastAlertEventId", f.long("lastAlertEventId"), s.lastAlertEventId)
            assertEquals("$name heartbeat", f.int("heartbeat"), s.heartbeat)
            assertEquals("$name schedule", f.int("schedule"), s.schedule)
            assertEquals("$name nextReportS", f.int("nextReportS"), s.nextReportS)
        }
    }

    @Test fun replies_decode_exact_fields() {
        for (c in root["replies"].arr()) {
            val m = c.obj(); val f = m["fields"].obj(); val name = m.str("name")
            when (val fr = LinkCodec.decodeFrame(hex(m.str("bytes")))) {
                is Frame.HelloAck -> {
                    assertEquals("$name reqId", f.int("reqId"), fr.reqId)
                    assertEquals("$name linkStatus", f.int("linkStatus"), fr.linkStatus)
                    assertEquals("$name serverMajor", f.int("serverMajor"), fr.serverMajor)
                    assertEquals("$name serverMinor", f.int("serverMinor"), fr.serverMinor)
                    assertEquals("$name sessionId", f.int("sessionId"), fr.sessionId)
                    assertEquals("$name capabilities", f.int("capabilities"), fr.capabilities)
                    assertEquals("$name maxFrame", f.int("maxFrame"), fr.maxFrame)
                }
                is Frame.Ack -> {
                    assertEquals("$name token", f.long("token"), fr.token)
                    assertEquals("$name heartbeat", f.int("heartbeat"), fr.heartbeat)
                }
                is Frame.Result -> {
                    assertEquals("$name commandType", f.int("commandType"), fr.commandType)
                    assertEquals("$name commandResult", f.int("commandResult"), fr.commandResult)
                }
                is Frame.EventSummary -> {
                    assertEquals("$name eventId", f.long("eventId"), fr.eventId)
                    assertEquals("$name detector", f.int("detector"), fr.detector)
                    assertEquals("$name confidence", f.int("confidence"), fr.confidence)
                    assertEquals("$name sourceKind", f.int("sourceKind"), fr.sourceKind)
                    assertEquals("$name band", f.int("band"), fr.band)
                    assertEquals("$name channel", f.int("channel"), fr.channel)
                    assertEquals("$name rssi", f.int("rssi"), fr.rssi)
                    assertEquals("$name count", f.int("count"), fr.count)
                    assertEquals("$name ageSeconds", f.int("ageSeconds"), fr.ageSeconds)
                    assertEquals("$name flags", f.int("flags"), fr.flags)
                }
                is Frame.End -> {
                    assertEquals("$name count", f.int("count"), fr.count)
                    assertEquals("$name gap", f.int("gap"), fr.gap)
                    assertEquals("$name changeSeq", f.long("changeSeq"), fr.changeSeq)
                }
                is Frame.Text -> {
                    assertEquals("$name field", f.int("field"), fr.field)
                    assertEquals("$name index", f.int("index"), fr.index)
                    assertEquals("$name total", f.int("total"), fr.total)
                    assertEquals("$name length", f.int("length"), fr.bytes.size)
                }
                is Frame.Error -> {
                    assertEquals("$name reqId", f.int("reqId"), fr.reqId)
                    assertEquals("$name linkStatus", f.int("linkStatus"), fr.linkStatus)
                }
                is Frame.Unknown -> error("$name decoded as Unknown")
            }
        }
    }

    @Test fun get_changed_run_reassembles_with_gap_and_deauth() {
        val byName = root["replies"].arr().associate { it.obj().str("name") to it.obj().str("bytes") }
        val summaries = ArrayList<Frame.EventSummary>()
        var end: Frame.End? = null
        for (n in listOf("event_summary_wifi_negative_rssi", "event_summary_ble_saturated", "end_after_two_with_gap")) {
            when (val fr = LinkCodec.decodeFrame(hex(byName.getValue(n)))) {
                is Frame.EventSummary -> summaries.add(fr)
                is Frame.End -> end = fr
                else -> {}
            }
        }
        assertEquals("two summaries", 2, summaries.size)
        assertEquals("END count matches", summaries.size, end!!.count)
        assertEquals("gap flag", 1, end!!.gap)
        assertEquals("first is DEAUTH", LinkConstants.ReconTarget.DEAUTH, summaries.first().detector)
    }

    @Test fun get_text_run_reassembles_in_order() {
        val byName = root["replies"].arr().associate { it.obj().str("name") to it.obj().str("bytes") }
        val text = listOf("text_first_fragment_full", "text_last_fragment_short")
            .map { LinkCodec.decodeFrame(hex(byName.getValue(it))) as Frame.Text }
            .sortedBy { it.index }
            .joinToString("") { String(it.bytes, Charsets.UTF_8) }
        assertTrue("reassembled text", text == "Free Public WiFi")
    }
}
