package com.projectrevivesolutions.layertime.controller.flow

import com.projectrevivesolutions.layertime.controller.wireshark.WiresharkDecoder

/**
 * The frame the analytic pipeline works on (2026-11-04): one decoded 802.11 frame
 * with the fields the checks read, nothing else. On the phone it is mapped from
 * Wireshark's dissection (fromPcap); in host tests the fabricator builds it
 * directly, so a scenario is exact. `index` points back at the frame in whatever
 * list it came from, which is how a finding opens Wireshark on the right frames.
 */
data class AnalyticFrame(
    val index: Int,
    val timeNanos: Long,
    val typeSubtype: Int,        // 802.11 type<<4 | subtype, Wireshark's wlan.fc.type_subtype
    val sa: String?,             // source / transmitter address
    val da: String?,             // destination / receiver address
    val bssid: String?,
    val seq: Int? = null,
    val retry: Boolean = false,
    val reasonCode: Int? = null, // deauth / disassoc
    val rssi: Int? = null,       // dBm from radiotap
    val channel: Int? = null,
    val ssid: String? = null,    // beacon / probe response
    val mfpCapable: Boolean? = null,   // RSN capabilities in a beacon; null when no RSN element
    val mfpRequired: Boolean? = null,
    val eapol: Boolean = false,
    val eapolMsg: Int? = null,   // EAPOL-Key handshake message number, wlan_rsna_eapol.keydes.msgnr
) {
    val isDeauth get() = typeSubtype == DEAUTH
    val isDisassoc get() = typeSubtype == DISASSOC
    val isDisconnect get() = isDeauth || isDisassoc
    val isBeacon get() = typeSubtype == BEACON
    val isData get() = (typeSubtype shr 4) == 2
    val isBroadcast get() = da.equals(BROADCAST, ignoreCase = true)

    companion object {
        const val BEACON = 0x08
        const val PROBE_RESP = 0x05
        const val DISASSOC = 0x0A
        const val DEAUTH = 0x0C
        const val DATA = 0x20
        const val QOS_DATA = 0x28
        const val BROADCAST = "ff:ff:ff:ff:ff:ff"

        /** From Wireshark's dissection. Fields Wireshark did not emit stay null. */
        fun fromPcap(index: Int, f: WiresharkDecoder.PcapFrame): AnalyticFrame {
            fun first(n: String) = f.first(n)?.display
            fun leadingInt(s: String?): Int? = s?.trim()?.let { t ->
                val hex = Regex("^0x([0-9a-fA-F]+)").find(t)?.groupValues?.get(1)
                if (hex != null) hex.toInt(16) else Regex("^-?\\d+").find(t)?.value?.toInt()
            }
            fun bit(s: String?): Boolean? = s?.trim()?.let { it == "1" || it.startsWith("1") || it.equals("true", true) || it.contains("Set", true) }
            val ts = leadingInt(first("wlan.fc.type_subtype")) ?: -1
            return AnalyticFrame(
                index = index,
                timeNanos = f.epochSeconds * 1_000_000_000L + f.nanos,
                typeSubtype = ts,
                sa = (first("wlan.sa") ?: first("wlan.ta"))?.lowercase(),
                da = (first("wlan.da") ?: first("wlan.ra"))?.lowercase(),
                bssid = first("wlan.bssid")?.lowercase(),
                seq = leadingInt(first("wlan.seq")),
                retry = bit(first("wlan.fc.retry")) ?: false,
                reasonCode = leadingInt(first("wlan.fixed.reason_code")),
                rssi = leadingInt(first("radiotap.dbm_antsignal")),
                channel = leadingInt(first("wlan_radio.channel")) ?: leadingInt(first("radiotap.channel.freq"))?.let { channelOf(it) },
                ssid = first("wlan.ssid"),
                mfpCapable = bit(first("wlan.rsn.capabilities.mfpc")),
                mfpRequired = bit(first("wlan.rsn.capabilities.mfpr")),
                eapol = f.fields.any { it.name == "eapol" },
                eapolMsg = leadingInt(first("wlan_rsna_eapol.keydes.msgnr")),
            )
        }

        fun channelOf(freqMhz: Int): Int? = when {
            freqMhz == 2484 -> 14
            freqMhz in 2412..2472 -> (freqMhz - 2407) / 5
            freqMhz in 5180..5885 -> (freqMhz - 5000) / 5
            else -> null
        }
        fun freqOf(channel: Int): Int = when {
            channel == 14 -> 2484
            channel in 1..13 -> 2407 + channel * 5
            else -> 5000 + channel * 5
        }
    }
}

/** The Hunter's detection as the pipeline sees it. */
data class DetectionEvent(
    val detector: String,
    val address: String?,     // the transmitter the detector named
    val channel: Int?,
    val rssi: Int?,
    val timeNanos: Long,
    val count: Int = 1,
)

/** Where frames come from: a capture file decoded on the phone, a fixture, or later the Hunter's evidence window. */
fun interface FrameSource {
    fun frames(): List<AnalyticFrame>
}
