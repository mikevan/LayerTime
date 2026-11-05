package com.projectrevivesolutions.layertime.controller.wireshark

/**
 * Plain explanations of 802.11 frame types for the Wireshark window (Michael, 2026-11-04).
 * Pure, host-tested.
 *
 * Keyed on the exact names Wireshark 4.4.19 gives each type and subtype
 * (packet-ieee80211.c, frame_type_subtype_vals), which is what a frame card's title
 * shows. A data frame that carries an EAPOL key is explained by its handshake message
 * number instead (wlan_rsna_eapol.keydes.msgnr). Every explanation is one or two full
 * sentences, describes what the frame does, and never claims what an attacker did.
 */
object FrameGlossary {

    private const val OLD_PCF = "A data frame from the old point-coordination mode, which current equipment almost never uses."
    private const val SIXTY_GHZ = "A 60 GHz (802.11ad or 802.11ay) control frame. The Hunter does not listen at 60 GHz, so this came from a capture made with other equipment."

    /** Wireshark's name for the type and subtype, mapped to its explanation. */
    val BY_NAME: Map<String, String> = linkedMapOf(
        // Management
        "Association Request" to "A station asks an access point to join its network, after authenticating. It lists the speeds and security it supports.",
        "Association Response" to "The access point answers an association request. A status of 0 means the station joined; any other status means it was refused.",
        "Reassociation Request" to "A station already on the network asks to move to another access point of the same network, or to rejoin after a short drop.",
        "Reassociation Response" to "The access point answers a reassociation request, accepting or refusing the move.",
        "Probe Request" to "A device asks which networks are nearby, either any network or one by name. A named probe can reveal a network the device has joined before.",
        "Probe Response" to "An access point answers a probe request with its network name and settings, the same information a beacon carries.",
        "Measurement Pilot" to "A short, frequent beacon-like frame some access points send so stations can find and measure them quickly.",
        "Beacon frame" to "An access point announces its network several times a second: its name, channel, and security. Every Wi-Fi network sends these, so beacons alone are normal.",
        "ATIM" to "Announces buffered traffic in an ad hoc (device-to-device) network that uses power saving.",
        "Disassociate" to "Tells a station it is no longer associated, though it stays authenticated. Like deauthentication it can be real or forged, and a burst of them is an attack pattern.",
        "Authentication" to "The first step of joining a network. On WPA2 it is an open formality and the real check is the EAPOL handshake after association; WPA3 uses this step for its SAE password exchange.",
        "Deauthentication" to "Ends a station's connection. Access points send it for real reasons, but most networks do not protect it, so anyone can forge it; a burst of them is how a deauth attack knocks clients off a network.",
        "Action" to "A management message for a network feature, such as a channel switch, a radio measurement, or block acknowledgement setup. Its category field says which.",
        "Action No Ack" to "An action frame that expects no acknowledgement, used for frequent reports such as beamforming feedback.",
        "Aruba Management" to "A vendor management frame that Aruba access points use among themselves.",
        // Control
        "Trigger" to "In Wi-Fi 6 the access point tells several stations when and how to send, so they can transmit at the same time.",
        "TWT Ack" to "Acknowledges a frame sent in a target wake time window, the Wi-Fi 6 schedule that lets stations sleep between turns.",
        "Beamforming Report Poll" to "The access point asks a station for more of its beamforming feedback.",
        "VHT/HE/EHT/RANGING NDP Announcement" to "Announces a sounding frame the access point uses to measure the radio path for beamforming, or a ranging exchange that measures distance.",
        "Control Wrapper" to "Carries another control frame with extra fields added.",
        "802.11 Block Ack Req" to "Asks the receiver to confirm which frames of a block it received.",
        "802.11 Block Ack" to "Confirms which frames of a block arrived, in one reply instead of one acknowledgement per frame.",
        "Power-Save poll" to "A station that was asleep asks the access point for the traffic it held.",
        "Request-to-send" to "A station reserves the air before sending a large frame. Others nearby stay quiet for the time it names.",
        "Clear-to-send" to "Grants a request-to-send and tells every station nearby to stay quiet. A station can also send one to itself to protect its own transmission.",
        "Acknowledgement" to "Confirms that one frame arrived intact. Every frame sent to a single station gets one.",
        "CF-End (Control-frame)" to "Ends a contention-free period or a reserved stretch of air time early.",
        "CF-End + CF-Ack (Control-frame)" to "Ends a contention-free period and acknowledges a frame at the same time.",
        "Poll" to SIXTY_GHZ, "Service Period Request" to SIXTY_GHZ, "Grant" to SIXTY_GHZ,
        "DMG Clear-to-send" to SIXTY_GHZ, "DMG Denial-to-send" to SIXTY_GHZ, "Grant Acknowledgment" to SIXTY_GHZ,
        "Sector Sweep" to SIXTY_GHZ, "Sector Sweep Feedback" to SIXTY_GHZ, "Sector Sweep Acknowledgment" to SIXTY_GHZ,
        // Data
        "Data" to "Carries network traffic. On a protected network the contents are encrypted and only the addresses can be read.",
        "Data + CF-Ack" to OLD_PCF, "Data + CF-Poll" to OLD_PCF, "Data + CF-Ack + CF-Poll" to OLD_PCF,
        "Null function (No data)" to "A data frame with no contents. Stations send it to tell the access point they are going to sleep or waking up, or to check that the link is still there.",
        "Acknowledgement (No data)" to OLD_PCF, "CF-Poll (No data)" to OLD_PCF, "CF-Ack/Poll (No data)" to OLD_PCF,
        "QoS Data" to "Carries network traffic with a priority: voice, video, best effort, or background. On a protected network the contents are encrypted.",
        "QoS Data + CF-Acknowledgment" to OLD_PCF, "QoS Data + CF-Poll" to OLD_PCF, "QoS Data + CF-Ack + CF-Poll" to OLD_PCF,
        "QoS Null function (No data)" to "A priority-tagged frame with no contents, used for power saving and link checks.",
        "QoS CF-Poll (No Data)" to OLD_PCF, "QoS CF-Ack + CF-Poll (No data)" to OLD_PCF,
    )

    /** The four WPA handshake messages, by Wireshark's message number. */
    val EAPOL_BY_MESSAGE: Map<Int, String> = mapOf(
        1 to "WPA handshake message 1 of 4. The access point sends a random number to start setting up this station's encryption keys.",
        2 to "WPA handshake message 2 of 4. The station answers with its own random number and a code that proves it knows the network password.",
        3 to "WPA handshake message 3 of 4. The access point proves it knows the password too, and sends the group key in encrypted form.",
        4 to "WPA handshake message 4 of 4. The station confirms, the keys are installed, and encrypted traffic can start.",
    )
    const val EAPOL_OTHER = "An EAPOL frame: part of the WPA key exchange that sets up a station's encryption keys."

    /** Wireshark's type/subtype name from a frame card title, "Deauthentication (0x000c)" to "Deauthentication". */
    fun typeName(title: String): String = title.substringBefore(" (0x").trim()

    /** The explanation for one frame. */
    fun explain(facts: FrameFacts): String {
        if (facts.eapol) return facts.eapolMessage?.let { EAPOL_BY_MESSAGE[it] } ?: EAPOL_OTHER
        return BY_NAME[facts.typeName]
            ?: "Wireshark names this frame ${facts.typeName}. LayerScope has no explanation for it yet."
    }
}

/** What the explanations and the summary read from one decoded frame. */
data class FrameFacts(
    val typeName: String,
    val transmitter: String? = null,
    val receiver: String? = null,
    val bssid: String? = null,
    val ssid: String? = null,
    val reason: String? = null,
    val statusCode: Int? = null,
    val eapol: Boolean = false,
    val eapolMessage: Int? = null,
    val malformed: Boolean = false,
    val timeNanos: Long? = null,
) {
    companion object {
        /** Reads the facts from Wireshark's fields. */
        fun of(f: WiresharkDecoder.PcapFrame): FrameFacts = FrameFacts(
            typeName = FrameGlossary.typeName(f.title),
            transmitter = (f.first("wlan.sa") ?: f.first("wlan.ta"))?.display,
            receiver = (f.first("wlan.da") ?: f.first("wlan.ra"))?.display,
            bssid = f.first("wlan.bssid")?.display,
            ssid = f.first("wlan.ssid")?.shown,
            reason = f.first("wlan.fixed.reason_code")?.shown,
            statusCode = f.first("wlan.fixed.status_code")?.display?.let { parseNumber(it) },
            eapol = f.fields.any { it.name == "eapol" },
            eapolMessage = f.first("wlan_rsna_eapol.keydes.msgnr")?.display?.let { parseNumber(it) },
            malformed = f.malformed,
            timeNanos = if (f.hasTimestamp) f.epochSeconds * 1_000_000_000L + f.nanos else null,
        )

        /** "0x0007", "7", or "Message number: 2" to its number; null when there is none. */
        fun parseNumber(text: String): Int? {
            val t = text.trim()
            if (t.startsWith("0x", ignoreCase = true)) return t.substring(2).toIntOrNull(16)
            return t.toIntOrNull() ?: Regex("(\\d+)").find(t)?.value?.toIntOrNull()
        }
    }
}
