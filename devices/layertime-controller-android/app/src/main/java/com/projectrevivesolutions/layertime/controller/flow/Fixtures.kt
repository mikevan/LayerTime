package com.projectrevivesolutions.layertime.controller.flow

/**
 * Named scenarios with the verdict each was built to mean (2026-11-04). The test is
 * "the pipeline says what the fixture was built to mean." Times are nanoseconds;
 * each scenario starts at T0 so windows are easy to reason about. Recorded captures
 * remain a second check; these are the first, because every knob here is known.
 */
object Fixtures {
    const val AP = "aa:bb:cc:00:00:01"
    const val CLIENT = "10:20:30:40:50:61"
    const val CLIENT2 = "10:20:30:40:50:62"
    const val TWIN = "de:ad:be:ef:00:01"
    const val SSID = "HomeNet"
    const val CH = 6
    const val T0 = 1_700_000_000_000_000_000L
    const val S = 1_000_000_000L
    const val MS = 1_000_000L

    data class Scenario(val name: String, val expected: Verdict, val event: DetectionEvent, val frames: List<AnalyticFrame>, val pcap: ByteArray,
                        val why: String)

    private fun event(address: String, atNanos: Long, count: Int = 1) =
        DetectionEvent("DEAUTH", address, CH, -55, atNanos, count)

    /** The AP's beacons every 100 ms from t0 for `seconds`, sequence numbers counting from `seqStart`. */
    private fun FrameFabricator.beacons(t0: Long, seconds: Int, seqStart: Int, rssi: Int = -48, mfp: Pair<Boolean, Boolean>? = null): Int {
        var seq = seqStart
        for (i in 0 until seconds * 10) { beacon(t0 + i * 100 * MS, AP, SSID, CH, rssi + (i % 3) - 1, seq, mfp); seq = (seq + 1) % 4096 }
        return seq
    }

    /** One deauth from the AP to one client, ordinary reason, in sequence with its beacons. */
    fun singleHousekeepingDeauth(): Scenario {
        val f = FrameFabricator()
        f.beacons(T0, 2, 100)                     // seq 100..119
        f.deauth(T0 + 1_050 * MS, AP, CLIENT, AP, reason = 3, seq = 111, channel = CH, rssi = -49)
        f.beacons(T0 + 2 * S, 2, 120)
        return Scenario("single_housekeeping_deauth", Verdict.NOT_WORRISOME, event(AP, T0 + 1_050 * MS), f.built, f.pcap(),
            "One frame, one client, an ordinary reason, numbered in step with the AP's own beacons.")
    }

    /** A burst of ordinary deauths from the AP itself: inactivity, in sequence, same signal. */
    fun legitimateBurst(): Scenario {
        val f = FrameFabricator()
        var seq = f.beacons(T0, 1, 500)
        val clients = listOf(CLIENT, CLIENT2, "10:20:30:40:50:63", "10:20:30:40:50:64", "10:20:30:40:50:65", "10:20:30:40:50:66", "10:20:30:40:50:67")
        clients.forEachIndexed { i, c -> f.deauth(T0 + 1 * S + i * 200 * MS, AP, c, AP, reason = 4, seq = seq + i, channel = CH, rssi = -48) }
        f.beacons(T0 + 3 * S, 1, seq + clients.size)
        return Scenario("legitimate_burst_inactivity", Verdict.WATCH, event(AP, T0 + 1 * S, clients.size), f.built, f.pcap(),
            "Enough frames to be a burst, but every other sign is ordinary: standard reason codes, the AP's own sequence counter, the AP's own signal.")
    }

    /** Forged broadcast flood: reason 7, sequence numbers off the AP's counter, different signal, open network. */
    fun broadcastFloodReason7(): Scenario {
        val f = FrameFabricator()
        f.beacons(T0, 2, 1000)                    // AP at 1000..1019
        var fake = 3000
        for (i in 0 until 40) { f.deauth(T0 + 2 * S + i * 50 * MS, AP, AnalyticFrame.BROADCAST, AP, reason = 7, seq = fake, channel = CH, rssi = -72); fake = (fake + 1) % 4096 }
        f.beacons(T0 + 4 * S, 2, 1040)
        return Scenario("broadcast_flood_reason7", Verdict.LIKELY_ATTACK, event(AP, T0 + 2 * S, 40), f.built, f.pcap(),
            "A sustained broadcast flood with the attack tools' default reason, numbered off a second radio's counter, at a different signal, on an open network.")
    }

    /** The flood, then a copy of the network appears on a new BSSID and a client starts a handshake with it. */
    fun floodThenEvilTwin(): Scenario {
        val f = FrameFabricator()
        f.beacons(T0, 2, 1000)
        var fake = 3000
        for (i in 0 until 40) { f.deauth(T0 + 2 * S + i * 50 * MS, AP, AnalyticFrame.BROADCAST, AP, reason = 7, seq = fake, channel = CH, rssi = -72); fake = (fake + 1) % 4096 }
        f.beacons(T0 + 4 * S, 1, 1040)
        for (i in 0 until 10) f.beacon(T0 + 5 * S + i * 100 * MS, TWIN, SSID, CH, -70, 10 + i)
        f.eapol(T0 + 6 * S, TWIN, CLIENT, TWIN, seq = 20, channel = CH, rssi = -70)
        f.eapol(T0 + 6 * S + 30 * MS, CLIENT, TWIN, TWIN, seq = 7, channel = CH, rssi = -60)
        return Scenario("flood_then_evil_twin", Verdict.ATTACK_WITH_FOLLOW_ON, event(AP, T0 + 2 * S, 40), f.built, f.pcap(),
            "The flood, then the same network name on a new BSSID and a password handshake with it: the deauth was the opener.")
    }

    /** The same flood against a network that requires management frame protection. */
    fun floodOnProtectedNetwork(): Scenario {
        val f = FrameFabricator()
        f.beacons(T0, 2, 1000, mfp = true to true)
        for (i in 0 until 5) f.data(T0 + 500 * MS + i * 200 * MS, CLIENT, AP, AP, seq = 30 + i, channel = CH, rssi = -58)
        var fake = 3000
        for (i in 0 until 40) { f.deauth(T0 + 2 * S + i * 50 * MS, AP, AnalyticFrame.BROADCAST, AP, reason = 7, seq = fake, channel = CH, rssi = -72); fake = (fake + 1) % 4096 }
        f.beacons(T0 + 4 * S, 2, 1040, mfp = true to true)
        for (i in 0 until 5) f.data(T0 + 4 * S + i * 300 * MS, CLIENT, AP, AP, seq = 40 + i, channel = CH, rssi = -58)
        return Scenario("flood_on_protected_network", Verdict.LIKELY_ATTACK, event(AP, T0 + 2 * S, 40), f.built, f.pcap(),
            "Every sign of an attack against a network that requires management frame protection, and the client kept talking: still called an attack, with a note that it most likely failed.")
    }

    /** Twenty forged deauths aimed at one client; the client goes quiet. */
    fun targetedFlood(): Scenario {
        val f = FrameFabricator()
        f.beacons(T0, 2, 1000)
        for (i in 0 until 5) f.data(T0 + 500 * MS + i * 200 * MS, CLIENT, AP, AP, seq = 40 + i, channel = CH, rssi = -58)
        var fake = 2200
        for (i in 0 until 20) { f.deauth(T0 + 2 * S + i * 100 * MS, AP, CLIENT, AP, reason = 7, seq = fake, channel = CH, rssi = -75); fake = (fake + 3) % 4096 }
        f.beacons(T0 + 4 * S, 2, 1040)
        return Scenario("targeted_flood", Verdict.LIKELY_ATTACK, event(AP, T0 + 2 * S, 20), f.built, f.pcap(),
            "A sustained flood aimed at one client with the attack tools' default reason, off the AP's counter, at a different signal; the client went quiet.")
    }

    /** A flood heard with no beacons from the AP at all: three checks cannot run. */
    fun floodWithoutBeacons(): Scenario {
        val f = FrameFabricator()
        var fake = 3000
        for (i in 0 until 40) { f.deauth(T0 + 2 * S + i * 50 * MS, AP, AnalyticFrame.BROADCAST, AP, reason = 7, seq = fake, channel = CH, rssi = -72); fake = (fake + 1) % 4096 }
        return Scenario("flood_without_beacons", Verdict.LIKELY_ATTACK, event(AP, T0 + 2 * S, 40), f.built, f.pcap(),
            "Broadcast, sustained, reason 7: enough on its own. Sequence, signal, and protection could not be checked without the AP's beacons, and the finding says so.")
    }

    fun all(): List<Scenario> = listOf(singleHousekeepingDeauth(), legitimateBurst(), broadcastFloodReason7(), floodThenEvilTwin(),
        floodOnProtectedNetwork(), targetedFlood(), floodWithoutBeacons())
}
