package com.projectrevivesolutions.layertime.controller.flow

import kotlin.math.abs
import kotlin.math.roundToInt

/**
 * The deauth triage steps (2026-11-04), from claude/layerscope_analytics_design.md:
 * seven deterministic checks and one verdict, each a runner registered beside its
 * StepSpec. Every feature carries one sentence for the operator and the frames it
 * rests on. A check that cannot run says so (Lean.UNKNOWN) instead of guessing.
 * Nothing here transmits, and no feature or finding ever says "countered".
 */
object DeauthSteps {
    private const val SEQ_MOD = 4096
    private const val SEQ_SLACK = 64           // how far past the last beacon a sequence may run and still be the AP's

    /** The frames a check sees: the window, the disconnects from the named transmitter, and the AP's own beacons. */
    class View(val window: List<AnalyticFrame>, event: DetectionEvent) {
        val deauths: List<AnalyticFrame> = window.filter { it.isDisconnect && (event.address == null || it.sa == event.address.lowercase()) }
            .sortedBy { it.timeNanos }
        val ap: String? = deauths.mapNotNull { it.bssid }.groupingBy { it }.eachCount().maxByOrNull { it.value }?.key
            ?: event.address?.lowercase()
        val beacons: List<AnalyticFrame> = window.filter { it.isBeacon && it.sa == ap }.sortedBy { it.timeNanos }
        val ssid: String? = beacons.firstNotNullOfOrNull { it.ssid }
        val lastDeauthNanos: Long? = deauths.lastOrNull()?.timeNanos
    }

    private fun frames(inputs: Map<String, List<Any>>): List<AnalyticFrame> =
        inputs["frames"]?.firstOrNull()?.let { @Suppress("UNCHECKED_CAST") (it as List<AnalyticFrame>) } ?: emptyList()

    private fun feature(step: String, value: String, lean: Lean, reason: String, frames: List<AnalyticFrame> = emptyList()) =
        Feature(step, StepRegistry.spec(step)?.title ?: step, value, lean, reason, frames.map { it.index })

    private fun noDeauths(step: String) = feature(step, "none", Lean.UNKNOWN, "No disconnect frames from this transmitter are in the window.")

    fun register() {
        // Detection gathers the frames in its window around the event; every check reads from here.
        Engine.register("event_source") { _, s, ctx ->
            val half = s.long("window_ms") * 1_000_000L
            ctx.source.frames().filter { it.timeNanos >= ctx.event.timeNanos - half && it.timeNanos <= ctx.event.timeNanos + half }
        }
        Engine.register("deauth_target") { inputs, _, ctx -> target(View(frames(inputs), ctx.event)) }
        Engine.register("deauth_rate") { inputs, s, ctx -> rate(View(frames(inputs), ctx.event), s.int("burst_frames"), s.long("burst_ms")) }
        Engine.register("deauth_reason") { inputs, _, ctx -> reason(View(frames(inputs), ctx.event)) }
        Engine.register("seq_integrity") { inputs, _, ctx -> sequence(View(frames(inputs), ctx.event)) }
        Engine.register("signal_match") { inputs, s, ctx -> signal(View(frames(inputs), ctx.event), s.int("tolerance_db")) }
        Engine.register("aftermath") { inputs, s, ctx -> aftermath(View(frames(inputs), ctx.event), s.long("watch_ms")) }
        Engine.register("mgmt_protection") { inputs, _, ctx -> protection(View(frames(inputs), ctx.event)) }
        Engine.register("deauth_verdict") { inputs, s, _ ->
            val features = inputs["features"]?.filterIsInstance<Feature>() ?: emptyList()
            @Suppress("UNCHECKED_CAST")
            val frames = (inputs["frames"]?.firstOrNull() as? List<AnalyticFrame>) ?: emptyList()
            val text = (s["rules"] as? String).orEmpty()
            val rules = if (text.isBlank()) VerdictRuleSet.deauthDefault() else VerdictRuleSet.parse(text)
            rules.evaluate(features, frames)
        }
    }

    fun target(v: View): Feature {
        if (v.deauths.isEmpty()) return noDeauths("target")
        val broadcast = v.deauths.filter { it.isBroadcast }
        if (broadcast.isNotEmpty()) return feature("target", "broadcast",
            Lean.SUSPICIOUS, "${broadcast.size} of ${v.deauths.size} disconnects were sent to every client at once.", broadcast)
        val clients = v.deauths.mapNotNull { it.da }.distinct()
        return if (clients.size == 1) feature("target", "one client", Lean.BENIGN, "Aimed at one client, ${clients[0]}.", v.deauths)
        else feature("target", "${clients.size} clients", Lean.BENIGN, "Sent to ${clients.size} different clients, each addressed on its own.", v.deauths)
    }

    fun rate(v: View, burstFrames: Int, burstMs: Long): Feature {
        if (v.deauths.isEmpty()) return noDeauths("rate")
        if (v.deauths.size == 1) return feature("rate", "1 frame", Lean.BENIGN, "A single disconnect frame; nothing sustained.", v.deauths)
        val span = (v.deauths.last().timeNanos - v.deauths.first().timeNanos) / 1e9
        val burstNs = burstMs * 1_000_000L
        var peak = 0
        var j = 0
        for (i in v.deauths.indices) {
            while (v.deauths[i].timeNanos - v.deauths[j].timeNanos > burstNs) j++
            peak = maxOf(peak, i - j + 1)
        }
        val value = "${v.deauths.size} frames over ${"%.1f".format(span)} s, peak $peak in ${burstMs / 1000.0} s"
        return if (peak >= burstFrames) feature("rate", value, Lean.SUSPICIOUS,
            "A burst: $peak disconnects inside ${burstMs / 1000.0} s, against a threshold of $burstFrames.", v.deauths)
        else feature("rate", value, Lean.BENIGN, "A few disconnects spread out; below the burst threshold of $burstFrames in ${burstMs / 1000.0} s.", v.deauths)
    }

    /** 802.11 reason codes 1 to 24 and 32 to 39 are the common standard ones; 0 and reserved values are a tell. */
    private fun standardReason(c: Int) = c in 1..24 || c in 32..39 || c in 45..47 || c in 52..66

    fun reason(v: View): Feature {
        val coded = v.deauths.filter { it.reasonCode != null }
        if (coded.isEmpty()) return noDeauths("reason")
        val hist = coded.groupingBy { it.reasonCode!! }.eachCount()
        val value = hist.entries.sortedByDescending { it.value }.joinToString(", ") { "${it.key} x${it.value}" }
        val seven = coded.filter { it.reasonCode == 7 }
        val illegal = coded.filter { !standardReason(it.reasonCode!!) }
        return when {
            illegal.isNotEmpty() -> feature("reason", value, Lean.SUSPICIOUS,
                "${illegal.size} disconnects give a reason code outside the standard (${illegal.map { it.reasonCode }.distinct().joinToString()}).", illegal)
            seven.size * 2 >= coded.size -> feature("reason", value, Lean.SUSPICIOUS,
                "${seven.size} of ${coded.size} give reason 7, the default of aireplay-ng and similar tools.", seven)
            else -> feature("reason", value, Lean.BENIGN, "Ordinary reason codes, the kind an access point uses for housekeeping.", coded)
        }
    }

    private fun dist(a: Int, b: Int) = ((b - a) % SEQ_MOD + SEQ_MOD) % SEQ_MOD

    fun sequence(v: View): Feature {
        if (v.deauths.isEmpty()) return noDeauths("seq")
        if (v.beacons.isEmpty()) return feature("seq", "no beacons", Lean.UNKNOWN,
            "No beacons from the access point were heard, so there is no sequence counter to compare against.")
        val numbered = v.deauths.filter { it.seq != null }
        if (numbered.isEmpty()) return feature("seq", "no numbers", Lean.UNKNOWN, "The disconnects carry no sequence numbers.")
        val odd = ArrayList<AnalyticFrame>()
        var lastSeq: Int? = null
        for (d in numbered) {
            val q = d.seq!!
            if (d.retry && q == lastSeq) { continue } // a retransmission repeats its number
            lastSeq = q
            val prev = v.beacons.lastOrNull { it.timeNanos <= d.timeNanos && it.seq != null }?.seq
            val next = v.beacons.firstOrNull { it.timeNanos >= d.timeNanos && it.seq != null }?.seq
            val fits = when {
                prev != null && next != null -> { val gap = dist(prev, next).let { if (it == 0) SEQ_MOD else it }; dist(prev, q) in 1..gap }
                prev != null -> dist(prev, q) in 1..SEQ_SLACK
                next != null -> dist(q, next) in 1..SEQ_SLACK
                else -> true
            }
            if (!fits) odd += d
        }
        val value = "${odd.size} of ${numbered.size} off the AP's counter"
        return if (odd.size * 2 >= numbered.size) feature("seq", value, Lean.SUSPICIOUS,
            "${odd.size} of ${numbered.size} disconnects carry sequence numbers that do not fit between the access point's own beacons: a second radio sent them.", odd)
        else feature("seq", value, Lean.BENIGN, "The disconnects are numbered in step with the access point's own beacons.", numbered)
    }

    fun signal(v: View, toleranceDb: Int): Feature {
        val d = v.deauths.mapNotNull { it.rssi }
        val b = v.beacons.mapNotNull { it.rssi }
        if (v.deauths.isEmpty()) return noDeauths("signal")
        if (b.isEmpty()) return feature("signal", "no beacons", Lean.UNKNOWN, "No beacons from the access point were heard, so there is no signal to compare against.")
        if (d.isEmpty()) return feature("signal", "no signal", Lean.UNKNOWN, "The disconnects carry no signal strength.")
        val md = d.average().roundToInt(); val mb = b.average().roundToInt()
        val diff = abs(md - mb)
        val value = "disconnects $md dBm, beacons $mb dBm"
        return if (diff > toleranceDb) feature("signal", value, Lean.SUSPICIOUS,
            "The disconnects arrive $diff dB apart from the access point's own beacons, past the $toleranceDb dB tolerance. Supporting evidence only.", v.deauths)
        else feature("signal", value, Lean.BENIGN, "The disconnects arrive at the same strength as the access point's own beacons, within $toleranceDb dB.", v.deauths)
    }

    fun aftermath(v: View, watchMs: Long): Feature {
        val last = v.lastDeauthNanos ?: return noDeauths("after")
        val until = last + watchMs * 1_000_000L
        val after = v.window.filter { it.timeNanos > last && it.timeNanos <= until }
        // Who was disconnected: the clients named in unicast deauths, plus, for a broadcast
        // deauth, every station that was sending data to this AP before the flood.
        val named = v.deauths.mapNotNull { it.da }.filter { it != AnalyticFrame.BROADCAST }.toSet()
        val firstDeauth = v.deauths.first().timeNanos
        val known = if (v.deauths.any { it.isBroadcast })
            v.window.filter { it.timeNanos < firstDeauth && it.isData && it.bssid == v.ap && it.sa != v.ap }.mapNotNull { it.sa }.toSet()
        else emptySet()
        val targets = named + known
        val talking = after.filter { it.isData && !it.eapol && it.sa in targets }
        val clones = after.filter { it.isBeacon && it.ssid != null && it.ssid == v.ssid && it.sa != v.ap }
        val handshakes = after.filter { it.eapol }
        val signs = ArrayList<String>()
        if (clones.isNotEmpty()) signs += "the network name reappeared on ${clones.map { it.sa }.distinct().size} new BSSID"
        if (handshakes.isNotEmpty()) signs += "a password handshake followed"
        if (talking.isNotEmpty()) signs += "the disconnected client kept transmitting (${talking.size} frames)"
        val value = if (signs.isEmpty()) "nothing" else listOfNotNull(
            if (clones.isNotEmpty()) "clone" else null, if (handshakes.isNotEmpty()) "handshake" else null,
            if (talking.isNotEmpty()) "client kept talking" else null).joinToString(", ")
        return if (signs.isEmpty()) feature("after", value, Lean.BENIGN, "Nothing followed in the next ${watchMs / 1000} s: no clone, no handshake, and no traffic from the disconnected clients.")
        else feature("after", value, Lean.SUSPICIOUS, "In the next ${watchMs / 1000} s, " + signs.joinToString("; ") + ".", clones + handshakes + talking)
    }

    fun protection(v: View): Feature {
        if (v.beacons.isEmpty()) return feature("mfp", "no beacons", Lean.UNKNOWN,
            "No beacons from the access point were heard, so its protection setting is unknown.")
        val required = v.beacons.any { it.mfpRequired == true }
        val capable = v.beacons.any { it.mfpCapable == true }
        return when {
            required -> feature("mfp", "required", Lean.BENIGN, "The network requires management frame protection; forged disconnects are dropped by its clients.", v.beacons.take(1))
            capable -> feature("mfp", "optional", Lean.SUSPICIOUS, "The network offers management frame protection but does not require it; clients that skipped it can be disconnected.", v.beacons.take(1))
            else -> feature("mfp", "none", Lean.SUSPICIOUS, "The network has no management frame protection; a forged disconnect works on every client.", v.beacons.take(1))
        }
    }

    /**
     * The verdict is produced by a rule set now (Michael, 2026-11-06). The shipped deauth
     * triage is [VerdictRuleSet.deauthDefault], so an operator can read it and copy it. This
     * wrapper keeps the old call working; callers that have no tile settings get that default.
     */
    fun verdict(features: List<Feature>): Finding = VerdictRuleSet.deauthDefault().evaluate(features)
}
