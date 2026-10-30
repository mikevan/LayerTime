package com.projectrevivesolutions.layertime.controller.ui

import com.projectrevivesolutions.layertime.controller.model.RssiSample
import kotlin.math.roundToInt

/**
 * The pure logic behind the QUICK LOOK card on ATTACK DETAIL: reading the
 * transmitter address the LayerWand reported, what kind of address it is, how
 * the signal is trending, and what the wearer should do. No Android types, so
 * it is unit-tested on the JVM (SignalAnalysisTest).
 *
 * Everything here works from what Link 0.1 already carries. Nothing is
 * inferred that the data cannot support: an address bit is a hint about the
 * address, not an identity, and a signal trend is a cue for walking toward or
 * away from something, not a distance.
 */
object SignalAnalysis {

    /** Link source kinds (contracts/vectors/enums.json). */
    const val SOURCE_WIFI = 1
    const val SOURCE_BLE = 2

    /** Six address bytes, most significant first, from "AA:BB:CC:DD:EE:FF" or "aa-bb-...". Null if it is not an address. */
    fun parseAddress(text: String?): ByteArray? {
        val parts = text?.trim()?.split(':', '-') ?: return null
        if (parts.size != 6) return null
        val out = ByteArray(6)
        for ((i, p) in parts.withIndex()) {
            if (p.length != 2) return null
            out[i] = (p.toIntOrNull(16) ?: return null).toByte()
        }
        return out
    }

    enum class AddressKind(val label: String, val meaning: String) {
        GLOBAL("Vendor-assigned",
            "A fixed address from the maker's registered block. It can identify the device model's vendor."),
        LOCAL("Randomized or locally set",
            "Phones and hotspots randomize their Wi-Fi addresses for privacy, so no vendor can be read from it and it may change."),
        GROUP("Group address",
            "A broadcast or multicast address, not one device."),
        BLE_UNKNOWN("Public or random (not reported)",
            "Bluetooth devices use either a fixed public address or a random one, and Link 0.1 does not say which. Most phones and trackers use random addresses."),
    }

    /** What kind of address this is. Wi-Fi addresses carry it in their own bits; Bluetooth ones do not. */
    fun addressKind(address: ByteArray, sourceKind: Int): AddressKind {
        if (sourceKind == SOURCE_BLE) return AddressKind.BLE_UNKNOWN
        val first = address[0].toInt() and 0xFF
        return when {
            first and 0x01 != 0 -> AddressKind.GROUP
            first and 0x02 != 0 -> AddressKind.LOCAL
            else -> AddressKind.GLOBAL
        }
    }

    enum class TrendState(val label: String) {
        CLOSER("Getting stronger"), FARTHER("Getting weaker"), STEADY("Steady"), TOO_FEW("Not enough readings yet")
    }

    data class Trend(val state: TrendState, val latest: Int, val strongest: Int, val readings: Int, val changeDb: Int)

    /** Readings closer together than this are one walk-test; older ones are ignored for the trend. */
    const val TREND_WINDOW_MS = 60_000L
    /** Bluetooth and Wi-Fi signal readings swing several dB from body position alone, so smaller changes read as steady. */
    const val TREND_THRESHOLD_DB = 6

    /**
     * The signal trend over the last [TREND_WINDOW_MS]: the latest reading
     * against the average of the earlier readings in the window. Needs at
     * least three readings in the window.
     */
    fun trend(history: List<RssiSample>, nowMillis: Long): Trend? {
        if (history.isEmpty()) return null
        val latest = history.last().rssi
        val strongest = history.maxOf { it.rssi }
        val recent = history.filter { nowMillis - it.timeMillis <= TREND_WINDOW_MS }
        if (recent.size < 3) return Trend(TrendState.TOO_FEW, latest, strongest, recent.size, 0)
        val earlier = recent.dropLast(1).map { it.rssi }.average()
        val change = (recent.last().rssi - earlier).roundToInt()
        val state = when {
            change >= TREND_THRESHOLD_DB -> TrendState.CLOSER
            change <= -TREND_THRESHOLD_DB -> TrendState.FARTHER
            else -> TrendState.STEADY
        }
        return Trend(state, latest, strongest, recent.size, change)
    }

    /** Seconds since the LayerWand last heard it: its age at the last report, plus the time since that report. */
    fun lastHeardSeconds(ageSeconds: Int, lastUpdatedMillis: Long, nowMillis: Long): Long =
        ageSeconds + ((nowMillis - lastUpdatedMillis).coerceAtLeast(0) / 1000)

    /** Heard within this long counts as still around. */
    const val ACTIVE_SECONDS = 60L

    /** One plain sentence on what the device is, and one on what to do. Detector numbers per contracts/vectors/enums.json. */
    fun advice(detector: Int): Pair<String, String> = when (detector) {
        5 -> "Something is knocking devices off Wi-Fi, often to grab a password handshake or push them onto a fake network." to
            "Do not rejoin Wi-Fi networks here. Use cellular data until it stops."
        6 -> "A Pwnagotchi collects Wi-Fi password handshakes from nearby networks." to
            "Make sure your own networks use WPA3 or a long, strong passphrase."
        7 -> "One access point is advertising several network names, a pattern used by fake hotspots and captive-portal traps." to
            "Do not join open networks here."
        8 -> "A Flock license-plate camera, or a device built around the same radio parts." to
            "Note the location. At Low confidence, this may be an ordinary laptop or gadget with the same Wi-Fi module."
        9 -> "A Wi-Fi Pineapple impersonates networks your devices have joined before." to
            "Turn off automatic joining of open networks."
        10 -> "An Apple Find My tracker, such as an AirTag." to
            "If it stays with you as you move, check your bags, clothing, and vehicle. Your phone can scan for unknown trackers."
        11 -> "A Flipper Zero, a hobbyist radio tool that can also spam Bluetooth pop-ups and copy access cards." to
            "Ignore unexpected pairing pop-ups. Keep access cards shielded."
        12 -> "Meta smart glasses, which can record video and audio." to
            "Assume you may be recorded."
        13 -> "An Axon body camera or related police equipment." to
            "Assume you may be recorded."
        14, 15, 16 -> "A Bluetooth item tracker." to
            "If it stays with you as you move, check your bags, clothing, and vehicle."
        else -> "A detection this build does not have a description for." to
            "Check the full details below."
    }
}
